#include "AResponseHandler.h"
#include "OrderProcess.h"
#include "gpbCommunication.h"
#include "server.h"

AResponseHandler::AResponseHandler(const AResponses & r) {
  for (int i = 0; i < r.arrived_size(); i++) {
    apurchasemores.push_back(std::move(r.arrived(i)));
    seqNums.push_back(r.arrived(i).seqnum());
  }

  for (int i = 0; i < r.ready_size(); i++) {
    apackeds.push_back(std::move(r.ready(i)));
    seqNums.push_back(r.ready(i).seqnum());
  }

  for (int i = 0; i < r.loaded_size(); i++) {
    aloadeds.push_back(std::move(r.loaded(i)));
    seqNums.push_back(r.loaded(i).seqnum());
  }

  // record acks from world
  for (int i = 0; i < r.acks_size(); i++) {
    Server & server = Server::getInstance();
    server.seqNumTable[r.acks(i)] = true;
  }
}

/*
  check whether given seqNum has been executed.If yes, return true,
  else return false. If given seqNum is not executed, record it in 
  the executed table.
*/
bool AResponseHandler::checkExecutedAndRecordIt(int seqNum) {
  // check whether this response has been executed

  Server & server = Server::getInstance();
  auto it = server.executeTable_World.find(seqNum);

  // if not exists, insert seqNum in the set, else exit
  if (it == server.executeTable_World.end()) {
    server.executeTable_World.insert(seqNum);
    return false;
  }
  else {
    return true;
  }
}

/*
    use different threads to handle different type of responses, and ack those messages.
*/
void AResponseHandler::handle() {
  cout << "Begin handling AResponse.."<< endl;
  // ACK responses to world.
  ACommands ac;
  for (int i = 0; i < seqNums.size(); i++) {
    ac.add_acks(i);
    ac.set_acks(i, seqNums[i]);
  }
  Server & server = Server::getInstance();
  server.worldQueue.push(ac);

  // use different threads to handle different responses.
  for (auto r : apurchasemores) {
    if (checkExecutedAndRecordIt(r.seqnum()) == false) {
      thread t(processPurchaseMore, r);
      t.detach();
    }
  }

  for (auto r : apackeds) {
    if (checkExecutedAndRecordIt(r.seqnum()) == false) {
      thread t(processPacked, r);
      t.detach();
    }
  }

  for (auto r : aloadeds) {
    if (checkExecutedAndRecordIt(r.seqnum()) == false) {
      thread t(processLoaded, r);
      t.detach();
    }
  }
}
