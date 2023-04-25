#include "AUResponseHandler.h"
#include "OrderProcess.h"
#include "gpbCommunication.h"
#include "server.h"

AUResponseHandler::AUResponseHandler(const UACommands & r) {
  for (int i = 0; i < r.connectedtoworld_size(); i++) {
    connectedtoworld_vec.push_back(std::move(r.connectedtoworld(i)));
    seqNums.push_back(r.connectedtoworld(i).seqnum());
  }
  for (int i = 0; i < r.destinationupdated_size(); i++) {
    destinationupdated_vec.push_back(std::move(r.destinationupdated(i)));
    seqNums.push_back(r.destinationupdated(i).seqnum());
  }
  for (int i = 0; i < r.truckarrived_size(); i++) {
    truckarrived_vec.push_back(std::move(r.truckarrived(i)));
    seqNums.push_back(r.truckarrived(i).seqnum());
  }
  for (int i = 0; i < r.orderdeparture_size(); i++) {
    orderdeparture_vec.push_back(std::move(r.orderdeparture(i)));
    seqNums.push_back(r.orderdeparture(i).seqnum());
  }
  for (int i = 0; i < r.orderdelivered_size(); i++) {
    orderdeliveredv_vec.push_back(std::move(r.orderdelivered(i)));
    seqNums.push_back(r.orderdelivered(i).seqnum());
  }
  // record acks from ups
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
bool AUResponseHandler::checkExecutedAndRecordIt(int seqNum) {
  // check whether this response has been executed

  Server & server = Server::getInstance();
  auto it = server.executeTable_Ups.find(seqNum);

  // if not exists, insert seqNum in the set, else exit
  if (it == server.executeTable_Ups.end()) {
    server.executeTable_Ups.insert(seqNum);
    return false;
  }
  else {
    return true;
  }
}

/*
    use different threads to handle different type of responses, and ack those messages.
*/
void AUResponseHandler::handle() {
  // ACK responses to UPS.
  AUCommands aucommand;
  for (int i = 0; i < seqNums.size(); i++) {
    aucommand.add_acks(i);
    aucommand.set_acks(i, seqNums[i]);
  }
  Server & server = Server::getInstance();
  server.upsQueue.push(aucommand);

  // use different threads to handle different responses.
  for (auto r : connectedtoworld_vec) {
    if (checkExecutedAndRecordIt(r.seqnum()) == false) {
      thread t(processUAConnectedToWorld, r);
      t.detach();
    }
  }
  for (auto r : destinationupdated_vec) {
    if (checkExecutedAndRecordIt(r.seqnum()) == false) {
      thread t(processUADestinationUpdated, r);
      t.detach();
    }
  }
  for (auto r : truckarrived_vec) {
    if (checkExecutedAndRecordIt(r.seqnum()) == false) {
      thread t(processUATruckArrived, r);
      t.detach();
    }
  }
  for (auto r : orderdeparture_vec) {
    if (checkExecutedAndRecordIt(r.seqnum()) == false) {
      thread t(processUAOrderDeparture, r);
      t.detach();
    }
  }

  for (auto r : orderdeliveredv_vec) {
    if (checkExecutedAndRecordIt(r.seqnum()) == false) {
      thread t(processUAOrderDelivered, r);
      t.detach();
    }
  }
}