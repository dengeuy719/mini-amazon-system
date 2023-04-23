#include "server.h"
#include "myException.h"
#include "socket.h"
#include "gpbCommunication.h"

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

Server::Server() :
  whNum(5), worldHost("127.0.0.1"), worldPort("23456"),
  worldID(), upsHost(""), upsPort(""), webPort(""),
  seqNum(0)
{
    cout << "Constructing server" << endl;
    // unique_ptr<connection> C(Server::connectDB("mini_amazon", "postgres", "passw0rd"));
    // setTableDefaultValue(C.get());
    // C->disconnect();
    
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
  //init worldID
  // if (worldID != -1) {
  //   aconnect.set_worldid(worldID);
  // }
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
  AResponses ar;
  if (!recvMesgFrom<AResponses>(ar, worldReader.get())) {
    return;
  }
  AResponseHandler handler(ar);
  handler.handle();
}

void Server::processReceivedUpsMessages() {
  UACommands ar;
  if (!recvMesgFrom<UACommands>(ar, upsReader.get())) {
    return;
  }
  AUResponseHandler handler(ar);
  handler.handle();
}

void Server::sendMessagesToWorld() {
  ACommands msg;
  worldQueue.wait_and_pop(msg);
  if (!sendMesgTo(msg, worldWriter.get())) {
    throw MyException("fail to send message in world.");
  }
}

void Server::sendMessagesToUps() {
  AUCommands msg;
  upsQueue.wait_and_pop(msg);
  if (!sendMesgTo(msg, upsWriter.get())) {
    throw MyException("fail to send message in ups.");
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


void Server::disConnectDB(connection * C) {
  C->disconnect();
}

void Server::run() {
  try{
    connectWorld();
    //need test
    connectUps();
    //connectWeb();
    while (true) {
      std::thread recvWorldThread(&Server::processReceivedWorldMessages, this);
      std::thread recvUpsThread(&Server::processReceivedUpsMessages, this);
      std::thread sendWorldThread(&Server::sendMessagesToWorld, this);
      std::thread sendUpsThread(&Server::sendMessagesToUps, this);
      recvWorldThread.join();
      recvUpsThread.join();
      sendWorldThread.join();
      sendUpsThread.join();
    }


  }
  catch (const std::exception & e) {
    cerr << e.what() << endl;;
    //close(upsFD);
    close(worldFD);
    return;
  }
}