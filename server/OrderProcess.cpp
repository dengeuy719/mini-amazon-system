#include "OrderProcess.h"

#include <string>

#include "socket.h"

/*
  select a warehouse, which is closest to the order address. return the selected warehouse index.
*/
int selectWareHouse(const vector<Warehouse> & whList, const Order & order) {
  int index = -1;
  int minDistance = INT_MAX;
  for (int i = 0; i < whList.size(); i++) {
    int delta_x = abs(whList[i].getX() - order.getAddressX());
    int delta_y = abs(whList[i].getY() - order.getAddressY());
    if ((delta_x * delta_x + delta_y * delta_y) < minDistance) {
      minDistance = delta_x * delta_x + delta_y * delta_y;
      index = i;
    }
  }
  return index;
}

/*
  parse order from string to Order object. save it to the database.
*/
void parseOrder(string msg, int client_fd) {
  cout << "successfully receive order request, begin processing it..\n";
  Order order(msg);

  // determine to use which warehouse.
  Server::Ptr server = Server::get_instance();
  vector<Warehouse> whList = server->getWhList();
  int whIndex = selectWareHouse(whList, order);
  order.setWhID(whList[whIndex].getID());
  
  // save order in DB
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
  saveOrderInDB(C.get(), order);
  Server::disConnectDB(C.get());
  order.showOrder();

  // send back ack info to front-end
  string ackResponse = "ACK";
  sendMsg(client_fd, ackResponse.c_str(), ackResponse.length());
  close(client_fd);

  processOrder(order);
}

/*
  check inventory for the given order. If enough, use different threads to 
  begin packing and ordering a truck. Otherwise, send APurchasemore command to 
  world.
*/
void processOrder(const Order & order) {
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
  cout << "check Inventory for order " << order.getPackId() << endl;
  Server & server = Server::getInstance();

  while (1) {
    try {
      // check inventory
      int itemId = order.getItemId();
      int itemAmt = order.getAmount();
      int packageId = order.getPackId();
      int upsid = order.getUPSId();
      int whID = order.getWhID();
      int version = -1;
      bool isEnough = checkInventory(C.get(), itemId, itemAmt, whID, version);
      if (isEnough == true) {
        decreaseInventory(C.get(), whID, itemAmt, itemId, version);
        Server::disConnectDB(C.get());
        cout << "Inventory is enough for order " << order.getPackId() << endl;

        //create new thread to send APack Command to the world and AOrderATruck to the UPS
        thread t1(packOrder, order);
        t1.detach();
        thread t2(callATruck, order);
        t2.detach();
        return;
      }
      else {
        cout << "Inventory is not enough for order " << order.getPackId()
             << ", send APurchasemore to world" << endl;
        // save order
        server.order_lck.lock();
        server.orderQueue.push(order);
        server.order_lck.unlock();

        // create APurchasemore command
        ACommands ac;
        APurchaseMore * ap = ac.add_buy();
        ap->set_whnum(whID);
        AProduct * aproduct = ap->add_things();
        aproduct->set_id(itemId);
        aproduct->set_count(10 * itemAmt);
        aproduct->set_description(getDescription(C.get(), itemId));

        // add seqNum to this command.
        size_t seqNum = server.requireSeqNum();
        ap->set_seqnum(seqNum);

        // keep sending until get acked.
        pushWorldQueue(ac, seqNum);
        break;
      }
    }
    catch (const VersionErrorException & e) {
      std::cerr << e.what() << '\n';
    }
  }

  Server::disConnectDB(C.get());
}

/*
  Send APack command to the world to pack given order.
*/
void packOrder(Order order) {
  cout << "begin pack order " << order.getPackId() << endl;
  Server::Ptr server = Server::get_instance();
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  int whouse_id = order.getWhID();

  // create Apack command
  ACommands acommand;
  APack * apack = acommand.add_topack();
  apack->set_whnum(whouse_id);
  apack->set_shipid(order.getPackId());
  AProduct * aproduct = apack->add_things();
  aproduct->set_id(order.getItemId());
  aproduct->set_count(order.getAmount());
  aproduct->set_description(getDescription(C.get(), order.getItemId()));

  // add seqNum to this command.
  size_t seqNum = server->getSeqNum();
  apack->set_seqnum(seqNum);

  Server::disConnectDB(C.get());
  pushWorldQueue(acommand, seqNum);
  cout<<"already send Apack command to world.\n"; 
}

/*
  Send AOrderATruck command to UPS to assign a truck to delivery the given package.
*/
void callATruck(Order order) {
  cout << "begin call a truck for order " << order.getPackId() << endl;
  Server::Ptr server = Server::get_instance();

  // get warehouse information
  int whouse_x = -1;
  int whouse_y = -1;
  int whouse_id = order.getWhID();
  vector<Warehouse> whList = server->getWhList();
  for (auto wh : whList) {
    if (wh.getID() == whouse_id) {
      whouse_x = wh.getX();
      whouse_y = wh.getY();
      break;
    }
  }

  //create AOrderATruck Command
  AUCommand aucommand;
  AOrderATruck * aOrderTruck = aucommand.add_order();
  int dest_x = order.getAddressX();
  int dest_y = order.getAddressY();
  aOrderTruck->set_packageid(order.getPackId());
  aOrderTruck->set_warehouselocationx(whouse_x);
  aOrderTruck->set_warehouselocationy(whouse_y);
  aOrderTruck->set_warehouseid(whouse_id);
  aOrderTruck->set_destinationx(dest_x);
  aOrderTruck->set_destinationy(dest_y);
  if (order.getUPSId() != -1) {
    //why string upsid?
    string upsidString(to_string(order.getUPSId()));
    aOrderTruck->set_upsid(upsidString);
  }

  // add seqNum to this command.
  int seqNum = server->getSeqNum();
  aOrderTruck->set_seqnum(seqNum);

  pushUpsQueue(aucommand, seqNum);
  cout<<"already send AOrderATruck command to UPS.\n"; 
}

/* ------------------------ "Message Queue push functions" ------------------------ */
/*
  push AUCommand object into queue, and then send it to the UPS.
  It will keep pushing unitl server receives ack.
*/
void pushUpsQueue(const AUCommands & aucommand, int seqNum) {
  Server & server = Server::getInstance();
  while (1) {
    server.upsQueue.push(aucommand);
    this_thread::sleep_for(std::chrono::milliseconds(1000));
    if (server.seqNumTable[seqNum] == true)
      break;
  }
}

/*
  push ACommand object into queue, and then send it to the world.
  It will keep pushing unitl server receives ack.
*/
void pushWorldQueue(const ACommands & acommand, int seqNum) {
  Server & server = Server::getInstance();
  while (1) {
    server.worldQueue.push(acommand);
    this_thread::sleep_for(std::chrono::milliseconds(1000));
    if (server.seqNumTable[seqNum] == true)
      break;
  }
}

/* ------------------------ "Handle Response from World" ------------------------ */
/*
    create new thread, let it to process PurchaseMore
    (Go to the database to increase the corresponding product inventory in the corresponding warehouse)
*/
void processPurchaseMore(APurchaseMore r) {
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  // get warehouse id
  int whID = r.whnum();

  // get all products
  vector<AProduct> products;
  for (int i = 0; i < r.things_size(); i++) {
    products.push_back(std::move(r.things(i)));
  }

  // process each product purchasemore
  for (int i = 0; i < products.size(); i++) {
    int count = products[i].count();
    int productId = products[i].id();
    addInventory(C.get(), whID, count, productId);
    cout << "Purchased " << count << " products(" << productId
         << ") are arrived at warehouse " << whID << endl;
  }
  Server::disConnectDB(C.get());

  // process previous saved order
  Server & server = Server::getInstance();
  server.order_lck.lock();
  Order order = server.orderQueue.front();
  server.orderQueue.pop();
  server.order_lck.unlock();

  processOrder(order);
}

/*
    create new thread, let it to process Packed
    (Go to the database and change the order status to 'packed')
*/
void processPacked(APacked r) {
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  //get shipid
  int packageId = r.shipid();

  //process this order status to be 'packed'
  updatePacked(C.get(), packageId);
  cout << "Already pack order " << packageId << endl;

  Server::disConnectDB(C.get());
}

/*
    create new thread, let it to process Loaded
    (Send AStartDeliver to UPS, change order status as "loaded", when to set order
    status as delivering?)
*/
void processLoaded(ALoaded r) {
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  //update order status as "loaded"
  int packageId = r.shipid();
  updateLoaded(C.get(), packageId);
  cout << "Already load order " << packageId << endl;

  // Create AStartDeliver Command
  AUCommands aucommand;
  AStartDeliver * aStartDeliver = aucommand.add_deliver();
  aStartDeliver->set_packageid(packageId);

  // add seqNum to this command.
  Server & server = Server::getInstance();
  size_t seqNum = server.requireSeqNum();
  aStartDeliver->set_seqnum(seqNum);

  cout << "start deliver order " << packageId << endl;
  pushUpsQueue(aucommand, seqNum);
  updateDelivering(C.get(), packageId);
  
  Server::disConnectDB(C.get());
}

/* ------------------------ "Handle Response from UPS" ------------------------ */
/*
  receive UTruckArrive Response, keep checking database if the order status is packed 
  if order status is packed, then create APutOnTruck Command and send to World
*/
void processTruckArrived(UTruckArrive r) {
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  //get package id and truck id and warehouse id
  int packageId = r.packageid();
  int truckId = r.truckid();
  int whId = -1;
  cout << "truck "<<r.truckid()<<" arrive for order " << packageId <<" with seqNum: "<<r.seqnum() << endl;

  //check database if the order status is packed
  while (1) {
    if (checkOrderPacked(C.get(), packageId, whId)) {
      break;
    }
    this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  //create APutOnTruck Command
  cout << "start load order " << packageId <<" with seqNum: "<<r.seqnum()<< endl;
  ACommands acommand;
  APutOnTruck * aPutOnTruck = acommand.add_load();
  aPutOnTruck->set_whnum(whId);
  aPutOnTruck->set_shipid(packageId);
  aPutOnTruck->set_truckid(truckId);

  // add seqNum to this command.
  Server::Ptr server = Server::get_instance();
  size_t seqNum = server->getSeqNum();
  aPutOnTruck->set_seqnum(seqNum);

  Server::disConnectDB(C.get());
  pushWorldQueue(acommand, seqNum);
  
  updateLoading(C.get(), packageId);
  
}

/*
  receive UDelivered Response, change order status to be 'delivered'
*/
void processUDelivered(UDelivered r) {
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  //process this order status to be 'delivered'
  int packageId = r.packageid();
  updateDelivered(C.get(), packageId);
  cout << "Already delivered order " << packageId <<" with seqNum: "<<r.seqnum()<< endl;

  Server::disConnectDB(C.get());
}
