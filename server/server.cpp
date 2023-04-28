#include "server.h"


#include "socket.h"
#include "myException.h"
#include "sql_function.h"
#include "OrderProcess.h"


connection * Server::connectDB(string dbName, string userName, string password) {
  connection * C = new connection("dbname=" + dbName + " user=" + userName +
                                  " password=" + password);  
//   connection * C =
//       new connection("host=db port=5432 dbname=" + dbName + " user=" + userName +
//                      " password=" + password);  // use in docker
  if (!C->is_open()) {
    throw MyException("Cannot open database.");
  }
  
  return C;
}


void Server::disConnectDB(connection * C) {
  C->disconnect();
}


Server::Server() :
  whNum(5), worldHost("vcm-30541.vm.duke.edu"), worldPort("23456"),
  worldID(), upsHost(""), upsPort("9090"), webPort("8888"),
  seqNum(0)
{
    cout << "Constructing server" << endl;
    //unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
    connection * C = new connection("dbname=mini_amazon user=postgres password=passw0rd port=5432");  
//   connection * C =
//       new connection("host=db port=5432 dbname=" + dbName + " user=" + userName +
//                      " password=" + password);  // use in docker
    if (!C->is_open()) {
      throw MyException("Cannot open database.");
    }
    dropAllTable(C);
    createTable(C, "./database/tables.sql");
    insertSampleData(C);  //for testing
    //setTableDefaultValue(C.get());
    C->disconnect();
    
    for (size_t i = 0; i < MAX_SEQNUM; ++i) {
      seqNumTable.emplace_back(false);
    }
}

Server & Server::getInstance() {
    static Server s;
    return s;
}

/*
  init gpb buffer ptrs
*/
void Server::initializeWorldCommunication() {
  // unique_ptr<gpbIn> worldReader(new gpbIn(worldFD));
  // unique_ptr<gpbOut> worldWriter(new gpbOut(worldFD));
  worldReader = make_unique<gpbIn>(worldFD);
  worldWriter = make_unique<gpbOut>(worldFD);
}

void Server::initializeUpsCommunication() {
  // unique_ptr<gpbIn> upsReader(new gpbIn(upsFD));
  // unique_ptr<gpbOut> upsWriter(new gpbOut(upsFD));
  upsReader = make_unique<gpbIn>(upsFD);
  upsWriter = make_unique<gpbOut>(upsFD);
}

/*
  connect to world
*/
void Server::initAconnect(AConnect & aconnect) {
  //init initwh
  for (int i = 0; i < whNum; ++i) {
    AInitWarehouse * w = aconnect.add_initwh();
    w->set_id(i);
    w->set_x(i);
    w->set_y(i);
    whs.push_back(Warehouse(i, i, i));
  }
  //init isAmazon
  aconnect.set_isamazon(true);
}

void Server::AWHandshake(AConnect & aconnect, AConnected & aconnected) {
  //send AConnect 
  if (!sendMesgTo<AConnect>(aconnect, worldWriter.get())) {
    throw MyException("Cannot send AConnect command to world.");
  }
  //receive AConnected 
  if (!recvMesgFrom<AConnected>(aconnected, worldReader.get())) {
    throw MyException("Cannot recv AConnected command from world.");
  }
  //check AConnected
  if (aconnected.result() != "connected!") {
    throw MyException("Amazon-World handshake failed.");
  }
}

void Server::connectWorld() {

  //conncect to world
  worldFD = socketConnect(worldHost, worldPort);
  initializeWorldCommunication();

  //init Aconnect
  AConnect aconnect;
  initAconnect(aconnect);

  //Amazon-World handshake
  AConnected aconnected;
  AWHandshake(aconnect, aconnected);
  worldID = aconnected.worldid();

  cout << "Successfully connecting to world " << endl;
}

/*     
  connect to ups           
*/
void Server::initAUConnectCommand(AUCommands & aucommand) {
  //init AUConnectedToWorld
  AUConnectedToWorld * auconnect = aucommand.add_connectedtoworld();
  auconnect->set_worldid(worldID);
  size_t seqNum = requireSeqNum();
  cout << "**<seqNum used>** AUConnectedToWorld: " << seqNum << endl;
  auconnect->set_seqnum(seqNum);

  //aucommand.add_acks(seqNum);
}

// void Server::AUHandshake(AUCommands & aucommand, UACommands & uacommand) {
//   //send AUCommands 
//   unique_ptr<gpbOut> gpbOutPtr(new gpbOut(upsFD));
//   if (!sendMesgTo<AUCommands>(aucommand, gpbOutPtr.get())) {
//     throw MyException("Cannot send AUCommands command to UPS.");
//   }
//   //receive UACommands 
//   unique_ptr<gpbIn> gpbInPtr(new gpbIn(upsFD));
//   if (!recvMesgFrom<UACommands>(uacommand, gpbInPtr.get())) {
//     throw MyException("Cannot recv UACommands command from UPS.");
//   }
//   //check UAConnectedToWorld
//   if (aucommand.acks_size() != 1 || aucommand.acks_size() != uacommand.acks_size() || aucommand.acks(0) != uacommand.acks(0)) {
//     throw MyException("Amazon-Ups handshake failed.");
//   }
// }

void Server::connectUps() {
  //GOOGLE_PROTOBUF_VERIFY_VERSION;
  
  int serverFD = buildServer(upsPort);
  string clientIP;
  upsFD = serverAccept(serverFD, clientIP);
  initializeUpsCommunication();
  //init AUCommands
  AUCommands aucommand;
  initAUConnectCommand(aucommand);
  //push aucmd to upsQueue
  upsQueue.push(aucommand);

  // //Amazon-Ups handshake
  // UACommands uacommand;
  // AUHandshake(aucommand, uacommand);

}

/*
  handle send/revc gpb cmds
*/
void Server::processReceivedWorldMessages() {
  while(true) {
    AResponses ar;
    if (!recvMesgFrom<AResponses>(ar, worldReader.get())) {
      continue;
    }
    cout << "---- start receving AResponses ------" << endl;
    cout << ar.DebugString() << endl;
    // if (!(ar.acks_size() && ar.arrived_size() && ar.ready_size() && ar.loaded_size() && 
    //       ar.error_size() && ar.packagestatus_size())) {
    //         cout << "AResponses is empty"<< endl;
    //         cout << "---- finish receving AResponses ------" << endl;
    //         continue;
    //       }
    AResponseHandler handler(ar);
    handler.printAResponse();
    handler.handle();
  }
}

void Server::processReceivedUpsMessages() {
  while(true) {
    UACommands ar;
    if (!recvMesgFrom<UACommands>(ar, upsReader.get())) {
      continue;
    }
    cout << "---- start receving UACommands ------" << endl;
    cout << ar.DebugString() << endl;
    // if(!(ar.acks_size() && ar.connectedtoworld_size() && ar.destinationupdated_size() &&
    //       ar.truckarrived_size() && ar.orderdeparture_size() && ar.orderdelivered_size() &&
    //       ar.error_size())) {
    //         cout << "UACommands is empty"<< endl;
    //         cout << "---- finish receving UACommands ------" << endl;
    //         continue;
    //       }
    AUResponseHandler handler(ar);
    handler.printAUResponse();
    handler.handle();
  }
}

void Server::sendMessagesToWorld() {
  while(true) {
    ACommands msg;
    cout << " worldQueue.size() is: " << worldQueue.size() << endl;
    worldQueue.wait_and_pop(msg);
    printAMessage(msg);
    if (!sendMesgTo<ACommands>(msg, worldWriter.get())) {
      throw MyException("fail to send message in world.");
    }
    cout << "---- finish sending ACommands ------" << endl;
  }

}

void Server::sendMessagesToUps() {
  while(true) {
    AUCommands msg;
    upsQueue.wait_and_pop(msg);
    printAUMessage(msg);
    if (!sendMesgTo<AUCommands>(msg, upsWriter.get())) {
      throw MyException("fail to send message in ups.");
    }
    cout << "---- finish sending AUCommands ------" << endl;
  }
}

/*
  msg log
*/
void Server::printAUMessage(const AUCommands & msg) {
    cout << "---- start sending AUCommands ------" << endl;
    cout << msg.DebugString() << endl;
    // if (msg.connectedtoworld_size()){
    //   cout << "connectedtoworld: "  << msg.connectedtoworld_size()  << endl;  
    // }
    // if(msg.ordercreated_size()) {
    //   cout << "ordercreated: "  << msg.ordercreated_size()  << endl;
    // }
    // if(msg.requesttruck_size()){
    //   cout << "requesttruck: "  << msg.requesttruck_size()  << endl;
    // }
    // if(msg.orderloaded_size()){
    //   cout << "orderloaded: "  << msg.orderloaded_size()  << endl;
    // }
    // if(msg.acks_size()){
    //   cout << "acks: "  << msg.acks_size() << " : ";
    //   for(auto ack : msg.acks()) {
    //     cout << ack << " ";
    //   }
    // }
    
}

void Server::printAMessage(const ACommands & msg) {
    cout << "---- start sending ACommands ------" << endl;
    cout << msg.DebugString() << endl;
    // if(msg.buy_size()) cout << "buy: "  << msg.buy_size()  << endl;
    // if(msg.topack_size()) cout << "topack: "  << msg.topack_size()  << endl;
    // if(msg.load_size()) cout << "load: "  << msg.load_size()  << endl;
    // if(msg.queries_size()) cout << "quueries: "  << msg.queries_size()  << endl;
    // if(msg.acks_size()){
    //   cout << "acks: "  << msg.acks_size() << " : ";
    //   for(auto ack : msg.acks()) {
    //     cout << ack << " ";
    //   }
    // }
  
}

/*
  seqNum counter
*/
size_t Server::requireSeqNum() {
  lock_guard<mutex> lck(seqNum_lock);
  size_t num = seqNum;
  ++seqNum;
  return num;
}

/*
  Web communication
*/
void Server::processOrderFromWeb(const int serverFD) {
    // wait to accept connection.
    cout << "waiting for order from web.." << endl;
    while(true) {
      int webFD;
      string clientIP;
      string msg;
      try {
        webFD = serverAccept(serverFD, clientIP);
        msg = recvMsg(webFD);
        //processOrder(msg, webFD);
        thread t(processOrder, msg, webFD);
        t.detach();
      }
      catch (const std::exception & e) {
        std::cerr << e.what() << '\n';
      }
    }
    cout << "finishing processOrderFromWeb"<<endl;
}

void Server::run() {
  try{
    connectWorld();
    connectUps();
 
    thread recvWorldThread(&Server::processReceivedWorldMessages, this);
    recvWorldThread.detach();
    thread recvUpsThread(&Server::processReceivedUpsMessages, this);
    recvUpsThread.detach();
    thread sendWorldThread(&Server::sendMessagesToWorld, this);
    sendWorldThread.detach();
    thread sendUpsThread(&Server::sendMessagesToUps, this);
    sendUpsThread.detach();

    int serverFD = buildServer(webPort);
    processOrderFromWeb(serverFD);
  }
  catch (const std::exception & e) {
    cerr << e.what() << endl;;
    close(upsFD);
    close(worldFD);
    return;
  }
}