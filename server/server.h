#ifndef _SERVER_H
#define _SERVER_H

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <pqxx/pqxx>
#include <iostream>
#include <string>
#include <thread>

#include "ThreadSafe_queue.h"
#include "AResponseHandler.h"
#include "AUResponseHandler.h"
#include "lib/build/gen/world_amazon.pb.h"
#include "lib/build/gen/amazon_ups.pb.h"
#include "warehouse.h"
#include "Order.h"

using namespace std;
using namespace pqxx;

#define MAX_SEQNUM 65536

class Server {
    private:
        string webPort;
        string worldPort;
        string worldHost;
        string upsPort;
        string upsHost;
        
        size_t worldID;
        int worldFD;
        int upsFD;

        int whNum;
        vector<Warehouse> whs;
        
        //handle gpb msg
        unique_ptr<gpbIn> worldReader;
        unique_ptr<gpbOut> worldWriter;
        unique_ptr<gpbIn> upsReader;
        unique_ptr<gpbOut> upsWriter;
        //init 
        void initializeWorldCommunication();
        void initializeUpsCommunication();
        

        
        //connect world
        void connectWorld();
        void initAconnect(AConnect & aconnect);
        void AWHandshake(AConnect & aconnect, AConnected & aconnected);

        //connect ups
        void connectUps();
        void initAUConnectCommand(AUCommands & aucommand);
        void AUHandshake(AUCommands & aucommand, UACommands & uacommand);

        void processReceivedWorldMessages();
        void processReceivedUpsMessages();
        void sendMessagesToWorld();
        void sendMessagesToUps();


        Server();
        Server(Server &) = delete;
        Server & operator=(const Server &) = delete;

    public:   
        //manage seqNum
        vector<bool> seqNumTable;  // record whether commands with specific seqNum is acked.
        int seqNum;          // next seqNum to be used.
        mutex seqNum_lock;          // mutex used to lock seqNum

        // Records whether a response with a specific sequence number is executed
        // if seqNum is in executeTable, this response has been executed.
        unordered_set<int> executeTable_World;
        unordered_set<int> executeTable_Ups;

        //message queue, transfer message to sending threads
        ThreadSafe_queue<ACommands> worldQueue;
        ThreadSafe_queue<AUCommands> upsQueue;

        //order queue. save orders for later processing
        queue<Order> orderQueue;
        mutex order_lck;

        static Server & getInstance();
        size_t requireSeqNum();
        static connection * connectDB(string dbName, string userName, string password);
        static void disConnectDB(connection * C);
        void run();
};

#endif