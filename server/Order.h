#ifndef _ORDER_H
#define _ORDER_H
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

class Order {
 private:
  int address_x;
  int address_y;
  int amount;
  int item_id;
  float price;
  string item_description;
  string customer_name;
  int UPS_ID;
  int pack_id;
  int wh_id;


 public:
  Order() {}
  /*
    Parse the incoming orderInfo string from the front end to create the object.
  */
  Order(const string & str) {
    //the order input string format is as following:
    //addrx:addry:amount:itemID:itemPrice:UPSID:customerName
    //res[0]:res[1]:res[2]:res[3]:res[4]:res[5]:res[6]
    vector<string> res;
    stringstream input(str);
    string temp;
    const char pattern = ':';
    while (getline(input, temp, pattern)) {
      res.push_back(temp);
    }
    //Because the UPSID is optional, we need to judge if the UPSID is exists
    //if not exists, we set UPS_ID as -1 to indicate that
    if (res[7] == "") {
      UPS_ID = -1;
    }
    else {
      UPS_ID = stoi(res[7]);
    }
    address_x = stoi(res[0]);
    address_y = stoi(res[1]);
    amount = stoi(res[2]);
    item_id = stoi(res[3]);
    price = stof(res[4]);
    item_description = res[5];
    customer_name = res[6];
    wh_id = -1;
  }
  int getAddressX() const { return address_x; }
  int getAddressY() const { return address_y; }
  int getAmount() const { return amount; }
  int getItemId() const { return item_id; }
  float getPrice() const { return price; }
  int getUPSId() const { return UPS_ID; }
  string getDescription() const { return item_description; }
  string getCustomerName() const {return customer_name;}
  void setPackId(int packid) { pack_id = packid; }
  int getPackId() const { return pack_id; }
  void setWhID(int whID) { wh_id = whID; }
  int getWhID() const { return wh_id; }
  void showOrder() const {
    cout << "------------------------\n";
    cout << "package_id: " << pack_id << endl;
    cout << "item_id: " << item_id << endl;
    cout << "amount: " << amount << endl;
    cout << "address_x: " << address_x << endl;
    cout << "address_y: " << address_y << endl;
    cout << "price: " << price << endl;
    cout << "wh_id: " << wh_id << endl;
    cout << "item_description: " << item_description << endl;
    cout << "UPS_ID: " << UPS_ID << endl;
    cout << "customer name: " << customer_name <<endl;
    cout << "------------------------\n";
  }
};

#endif