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
  worldID(), upsHost(""), upsPort(""), webPort("8888"),
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
    //insertSampleData(C);  //for testing
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
  worldReader = make_unique<gpbIn>(worldFD);
  worldWriter = make_unique<gpbOut>(worldFD);
}

void Server::initializeUpsCommunication() {
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
    whs.emplace_back(Warehouse(i, i, i));
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
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  //conncect to world
  worldFD = socketConnect(worldHost, worldPort);
  initializeWorldCommunication();
  //int serverFD = buildServer("2345");

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
  AUConnectedToWorld auconnect;
  auconnect.set_worldid(worldID);
  size_t seqNum = requireSeqNum();
  auconnect.set_seqnum(seqNum);

  //add AUConnectedToWorld to AUCommands
  aucommand.add_connectedtoworld()->CopyFrom(auconnect);
  aucommand.add_acks(seqNum);
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
      return;
    }
    AResponseHandler handler(ar);
    handler.handle();
  }
}

void Server::processReceivedUpsMessages() {
  while(true) {
    UACommands ar;
    if (!recvMesgFrom<UACommands>(ar, upsReader.get())) {
      return;
    }
    AUResponseHandler handler(ar);
    handler.handle();
  }
}

void Server::sendMessagesToWorld() {
  while(true) {
    ACommands msg;
    worldQueue.wait_and_pop(msg);
    if (!sendMesgTo(msg, worldWriter.get())) {
      throw MyException("fail to send message in world.");
    }
  }

}

void Server::sendMessagesToUps() {
  while(true) {
    AUCommands msg;
    upsQueue.wait_and_pop(msg);
    if (!sendMesgTo(msg, upsWriter.get())) {
      throw MyException("fail to send message in ups.");
    }
  }
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
    cout << "processOrderFromWeb.." << endl;
    while(true) {
      int webFD;
      string clientIP;
      string msg;
      try {
        webFD = serverAccept(serverFD, clientIP);
        msg = recvMsg(webFD);
        processOrder(msg, webFD);
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
    //need test
    //connectUps();
    //connectWeb;
    //cout << "starting"
    int serverFD = buildServer(webPort);
    processOrderFromWeb(serverFD);
    
    thread recvWorldThread(&Server::processReceivedWorldMessages, this);
    recvWorldThread.detach();
    // thread recvUpsThread(&Server::processReceivedUpsMessages, this);
    // recvUpsThread.detach();
    thread sendWorldThread(&Server::sendMessagesToWorld, this);
    sendWorldThread.detach();
    // thread sendUpsThread(&Server::sendMessagesToUps, this);
    // sendUpsThread.detach();
  }
  catch (const std::exception & e) {
    cerr << e.what() << endl;;
    //close(upsFD);
    close(worldFD);
    return;
  }
}