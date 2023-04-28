#include "OrderProcess.h"
#include "sqlOperation.h"
#include "warehouse.h"
#include "server.h"
#include "socket.h"
#include "myException.h"

#include <vector>
#include <string>
/*
  select a warehouse, which is closest to the order address. return the selected warehouse index.
*/
int selectWareHouse(const vector<Warehouse> & whList, int order_addr_x, int order_addr_y) {
  int index = -1;
  int minDistance = INT_MAX;
  for (int i = 0; i < whList.size(); i++) {
    int delta_x = abs(whList[i].getX() - order_addr_x);
    int delta_y = abs(whList[i].getY() - order_addr_y);
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
// void parseOrder(string msg, int client_fd) {
//   unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
//   cout << "successfully receive order request, begin processing it..\n";
//   int orderID = stoi(msg);

//   // determine to use which warehouse.
//   Server & server = Server::getInstance();
//   vector<Warehouse> whList = server.getWhList();
//   int order_addr_x = getOrder_x(C.get(),orderID);
//   int order_addr_y = getOrder_y(C.get(),orderID);
//   int whIndex = selectWareHouse(whList, order_addr_x, order_addr_y);
//   setPackagesWhID(C.get(),orderID, whList[whIndex].getID());

//   // send back ack info to front-end
//   string ackResponse = "ACK";
//   sendMsg(client_fd, ackResponse.c_str(), ackResponse.length());
//   close(client_fd);

//   processOrder(orderID);
// }

/*
  check inventory for the given order. If enough, use different threads to 
  begin packing and ordering a truck. Otherwise, send APurchasemore command to 
  world.
*/
void processOrder(string msg, int webID) {
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
  cout << "successfully receive order request, begin processing it.." << endl;
  cout << "OrderID is: "<< msg <<endl;
  int orderID = stoi(msg);
  
  // determine to use which warehouse.
  Server & server = Server::getInstance();
  vector<Warehouse> whList = server.getWhs();
  int order_addr_x = getOrderAddrx(C.get(), orderID);
  int order_addr_y = getOrderAddry(C.get(),orderID);
  int whIndex = selectWareHouse(whList, order_addr_x, order_addr_y);
  int whID = whList[whIndex].getID();
  setPackagesWhID(C.get(), orderID, whID);
  cout << "finishing selecting warehouse"<< endl;
  // send back ack info to front-end
  string ackResponse = "ACK";
  sendMsg(webID, ackResponse.c_str(), ackResponse.length());
  close(webID);
  cout << "finishing sending ACK to web"<< endl;
  // save order
  server.order_lck.lock();
  server.orderQueue.push(orderID);
  server.order_lck.unlock();

  // create APurchasemore command
  cout << "Starting create APurchasemore" << endl;
  ACommands ac;
  APurchaseMore * ap = ac.add_buy();
  ap->set_whnum(whID);
  vector<int> packageIDs = getPackageIDs(C.get(), orderID);
  //cout << "Starting create APurchasemore: finish getPackageIDs" << endl;
  // add seqNum to this command.
  size_t seqNum = server.requireSeqNum();
  cout << "**<seqNum used>** Apurchasemore: " << seqNum << endl;
  ap->set_seqnum(seqNum);
  for(auto packageID: packageIDs) {
    AProduct * aproduct = ap->add_things();
    aproduct->set_id(getPackageProductID(C.get(), packageID));
    aproduct->set_count(getPackageAmount(C.get(), packageID));
    aproduct->set_description(getPackageDesc(C.get(), packageID));
    ap->set_seqnum(seqNum);
  }

  pushWorldQueue(ac, seqNum);  
  Server::disConnectDB(C.get());
  cout << "Finishing receiving order request, begin processing it.." << endl;
}

/*
  Send APack command to the world to pack given order.
*/
void packOrder(int orderID, int whID) {
  cout << "begin pack order " << orderID << endl;
  Server & server = Server::getInstance();
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  // create Apack command
  ACommands acommand;
  size_t seqNum = server.requireSeqNum();
  cout << "**<seqNum used>** packOrder: " << seqNum << endl;
  vector<int> packageIDs = getPackageIDs(C.get(), orderID);
  for(auto packageID: packageIDs) {
    APack * apack = acommand.add_topack();
    apack->set_whnum(whID);
    apack->set_shipid(packageID);
    AProduct * aproduct = apack->add_things();
    aproduct->set_id(getPackageProductID(C.get(), packageID));
    aproduct->set_count(getPackageAmount(C.get(), packageID));
    aproduct->set_description(getPackageDesc(C.get(), packageID));
    // add seqNum to this command.
    apack->set_seqnum(seqNum);
  }
  cout << "inside packOrder :" << acommand.DebugString() << endl;
  pushWorldQueue(acommand, seqNum);
  Server::disConnectDB(C.get());
  cout<<"already send Apack command to world.\n"; 
}

/*
  Send AOrderATruck command to UPS to assign a truck to delivery the given package.
*/
void requestTruck(int orderID, int whID) {
  cout << "begin call a truck for order " << orderID << endl;
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
  Server & server = Server::getInstance();

  // get warehouse information
  int wh_x = -1;
  int wh_y = -1;
  vector<Warehouse> whList = server.getWhs();
  for (auto wh : whList) {
    if (wh.getID() == whID) {
      wh_x = wh.getX();
      wh_y = wh.getY();
      break;
    }
  }
  AUCommands aucommand;
  int seqNum = server.requireSeqNum();
  cout << "**<seqNum used>** requestTruck: " << seqNum << endl;
  //create AUOrderCreated Command
  AUOrderCreated * auordercreated = aucommand.add_ordercreated();
  auordercreated->set_orderid(orderID);
  auordercreated->set_destinationx(getOrderAddrx(C.get(), orderID));
  auordercreated->set_destinationy(getOrderAddry(C.get(), orderID));
  auordercreated->set_seqnum(seqNum);
  //create AOrderATruck Command
  AURequestTruck * aurequesttruck = aucommand.add_requesttruck();
  aurequesttruck->set_whnum(whID);
  aurequesttruck->set_x(wh_x);
  aurequesttruck->set_y(wh_y);
  aurequesttruck->set_seqnum(seqNum);

  pushUpsQueue(aucommand, seqNum);
  //cout << server.upsQueue.size() << endl;
  Server::disConnectDB(C.get());
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
    cout << " upsQueue.size() is: " << server.upsQueue.size() << endl;
    server.upsQueue.push(aucommand);
    cout << "pushed 1 AUCommands to upsQueue, upsQueue size is " << server.upsQueue.size()<< endl;
    this_thread::sleep_for(std::chrono::milliseconds(10000));
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
    //cout << " worldQueue.size() is: " << server.worldQueue.size() << endl;
    server.worldQueue.push(acommand);
    cout << "pushed 1 ACommands to worldQueue, ACommands is " << server.worldQueue.size()<< endl;
    cout << "~~ ACommand is :" << acommand.DebugString() << " ~~" << endl;
    this_thread::sleep_for(std::chrono::milliseconds(10000));
    if (server.seqNumTable[seqNum] == true)
      break;
  }
}

/* ------------------------ "Handle Response from World" ------------------------ */
/*
    create new thread, let it to process PurchaseMore
    (Go to the database to increase the corresponding product inventory in the corresponding warehouse)
*/
void processPurchaseMore(const APurchaseMore & r) {
  cout << "Begin handling AResponse: apurchasemores "<< endl;
  // get warehouse id
  int whID = r.whnum();
  // process previous saved order
  Server & server = Server::getInstance();
  server.order_lck.lock();
  int orderID = server.orderQueue.front();
  server.orderQueue.pop();
  server.order_lck.unlock();
  
  // packOrder(orderID, whID);
  // requestTruck(orderID, whID);
  thread t1(packOrder,orderID, whID);
  t1.detach();
  thread t2(requestTruck,orderID, whID);
  t2.detach();
}

/*
    create new thread, let it to process Packed
    (Go to the database and change the order status to 'packed')
*/
void processPacked(const APacked & r) {
  cout << "Begin handling AResponse: apackeds "<< endl;
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  //get shipid
  int packageId = r.shipid();

  //process this order status to be 'packed'
  updatepackPacked(C.get(), packageId);
  cout << "Already pack order " << packageId << endl;

  Server::disConnectDB(C.get());
}

/*
    create new thread, let it to process Loaded
    (Send AStartDeliver to UPS, change order status as "loaded", when to set order
    status as delivering?)
*/
void processLoaded(const ALoaded & r) {
  cout << "Begin handling AResponse: aloadeds "<< endl;
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  //update order status as "loaded"
  int packageId = r.shipid();
  updatepackLoaded(C.get(), packageId);
  cout << "Already load order " << packageId << endl;

  // create orderloaded: contain truckid, packageid
  AUCommands aucommand;
  AUOrderLoaded * auOrderLoaded = aucommand.add_orderloaded();
  auOrderLoaded->set_packageid(packageId);
  auOrderLoaded->set_orderid(getPackageOrderID(C.get(), packageId));
  auOrderLoaded->set_truckid(getPackageUpsID(C.get(), packageId));
  // add seqNum to this command.
  Server & server = Server::getInstance();
  size_t seqNum = server.requireSeqNum();
  cout << "**<seqNum used>** orderloaded: " << seqNum << endl;
  auOrderLoaded->set_seqnum(seqNum);

  cout << "start deliver order " << packageId << endl;
  pushUpsQueue(aucommand, seqNum);
  updatepackDelivering(C.get(), packageId);
  
  Server::disConnectDB(C.get());
}

/* ------------------------ "Handle Response from UPS" ------------------------ */
/*
  receive UTruckArrive Response, keep checking database if the order status is packed 
  if order status is packed, then create APutOnTruck Command and send to World
*/
void processUAConnectedToWorld(const UAConnectedToWorld & r) {
  cout << "Begin handling UAResponse: UAConnectedToWorld "<< endl;
  Server & server = Server::getInstance();
  if(r.worldid() != server.getWorldID()) {
    throw MyException("Error: Fail to connect to the same world");
  }
}

void processUADestinationUpdated(const UADestinationUpdated & r) {
  cout << "Begin handling UAResponse: UADestinationUpdated "<< endl;
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
  //change this order address
  int orderID = r.orderid();
  int new_x = r.destinationx();
  int new_y = r.destinationy();
  updateOrderAddr(C.get(), orderID, new_x, new_y);
  cout << "Updated order address to (" << new_x << ", " << new_y << ")" << endl;

  Server::disConnectDB(C.get());
}

void processUATruckArrived(const UATruckArrived & r) {
  cout << "Begin handling UAResponse: UATruckArrived "<< endl;
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  //gettruck id and warehouse id
  int truckId = r.truckid();
  int whId = r.whnum();
  cout << "truck "<<r.truckid()<<" arrive for order " << truckId <<" with seqNum: "<<r.seqnum() << endl;

  //get packed packages id
  vector<int> packedPackageIDs = getPackedPackageIDs(C.get(), whId);

  //create APutOnTruck Command
  cout << "start load order " << truckId <<" with seqNum: "<<r.seqnum()<< endl;
  ACommands acommand;
  Server & server = Server::getInstance();
  size_t seqNum = server.requireSeqNum();
  cout << "**<seqNum used>** APutOnTruck: " << seqNum << endl;
  for(const auto packedPackageID: packedPackageIDs) {
    APutOnTruck * aPutOnTruck = acommand.add_load();
    aPutOnTruck->set_whnum(whId);
    aPutOnTruck->set_shipid(packedPackageID);
    aPutOnTruck->set_truckid(truckId);
    aPutOnTruck->set_seqnum(seqNum);
    updatepackLoading(C.get(), packedPackageID);
  }
  // add seqNum to this command.
  Server::disConnectDB(C.get());
  pushWorldQueue(acommand, seqNum);
}

void processUAOrderDeparture(const UAOrderDeparture & r) {
  cout << "Begin handling UAResponse: UAOrderDeparture "<< endl;
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
  //process this order status to be 'delivering'
  int packageId = r.packageid();
  updatepackDelivering(C.get(), packageId);
  cout << "Delivering package " << packageId << endl;

  Server::disConnectDB(C.get());
}

/*
  receive UDelivered Response, change order status to be 'delivered'
*/
void processUAOrderDelivered(const UAOrderDelivered & r) {
  cout << "Begin handling UAResponse: UAOrderDelivered "<< endl;
  //Connect the database
  unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));

  //process this order status to be 'delivered'
  int packageId = r.packageid();
  updatepackDelivered(C.get(), packageId);
  cout << "Already delivered order " << packageId <<" with seqNum: "<<r.seqnum()<< endl;

  Server::disConnectDB(C.get());
}
