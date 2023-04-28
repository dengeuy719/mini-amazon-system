#ifndef _ARESPONSEHANDLER_H
#define _ARESPONSEHANDLER_H

#include <vector>
#include "lib/build/gen/world_amazon.pb.h"
#include "lib/build/gen/amazon_ups.pb.h"

using namespace std;

class AResponseHandler {
 private:
  vector<APurchaseMore> apurchasemores;
  vector<APacked> apackeds;
  vector<ALoaded> aloadeds;
  vector<int> seqNums;

 public:
  AResponseHandler(const AResponses & r);
  ~AResponseHandler() {}
  void handle();
  void printAResponse();

 private:
  bool checkExecutedAndRecordIt(int seqNum);
};

#endif