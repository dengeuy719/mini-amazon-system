#include "sql_function.h"



void insertSampleData(connection* C) {
    try {
        work W(*C);

        // Insert data into USER_PROFILE
        W.exec("INSERT INTO USER_PROFILE (USER_NAME, ADDRX, ADDRY, UPSID) VALUES (1, '123', '456', 'UPS001'), (2, '789', '101', 'UPS002');");

        // Insert data into PRODUCT
        W.exec("INSERT INTO PRODUCT (PRODUCT_ID, PRODUCT_NAME, PRODUCT_DESC, PRICE, CATALOG) VALUES (1, 'Product A', 'Description A', 19.99, 'Electronics'), (2, 'Product B', 'Description B', 29.99, 'Home Appliances');");

        // Insert data into ORDER
        W.exec("INSERT INTO \"ORDER\" (ORDER_ADDR_X, ORDER_ADDR_Y, TIME, ORDER_OWNER_ID, UPS_ID, PRICE) VALUES (111, 222, '12:34:56', 1, 1001, 49.99);");

        // Insert data into PACKAGE
        W.exec("INSERT INTO PACKAGE (PACKAGE_DESC, PACKAGE_OWNER_ID, AMOUNT, PRODUCT_ID, WH_ID, ORDER_ID, STATUS) VALUES ('Sample Package 1', 1, 3, 1, 101, 1, 'packing');");

        W.commit();
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return;
    }
}
/*
    read sql command from the file and then create tabel using connection *C.
    If fails, it will throw exception.
*/
void createTable(connection * C, string fileName) {
  cout << "Starting create new Tables..." << endl;
  string sql;
  ifstream ifs(fileName.c_str(), ifstream::in);
  if (ifs.is_open() == true) {
    string line;
    while (getline(ifs, line))
      sql.append(line);
  }
  else {
    throw MyException("fail to open file.");
  }

  work W(*C);
  W.exec(sql);
  W.commit();
  cout << "Finishing create new Tables..." << endl;
}

/*
    Drop all the table in the DataBase. Using for test.
*/
void dropAllTable(connection *C) {
  cout << "Starting Drop all the existed table..." << endl;
  work W(*C);

  W.exec("DROP TABLE IF EXISTS PACKAGE;");
  W.exec("DROP TABLE IF EXISTS \"ORDER\";");
  W.exec("DROP TABLE IF EXISTS PRODUCT;");
  W.exec("DROP TABLE IF EXISTS USER_PROFILE;");

  W.commit();
  cout << "Finishing Drop all the existed table..." << endl;
}

/*
    set table default value order, item, inventory version as 1
    and order time as now and order status as packing
*/
void setTableDefaultValue(connection * C) {
  work W(*C);
  string sql = "ALTER TABLE orders ALTER COLUMN version SET DEFAULT 1;ALTER TABLE "
               "inventory ALTER COLUMN version SET DEFAULT 1;"
               "ALTER TABLE orders "
               "ALTER COLUMN time SET DEFAULT CURRENT_TIME (0);"
               "ALTER TABLE orders ALTER COLUMN status set DEFAULT 'packing';";
  W.exec(sql);
  W.commit();
  cout << "set orders, inventory, item tables version column default value 1, and orders "
          "time column as now"
       << endl;
}

/*
    Check the order item amount in the inventory table, return boolean
    True means: enough inventory, False means: not enough inventory.
    return current version through reference.
*/
bool checkInventory(connection * C, int itemId, int itemAmount, int whID, int & version) {
  //create nontransaction object for SELECT operation
  nontransaction N(*C);

  // create sql statement, we need to select item amount from inventory table
  stringstream sql;
  sql << "SELECT ITEM_AMOUNT, VERSION FROM INVENTORY WHERE "
         "ITEM_ID= "
      << itemId << "AND WH_ID= " << whID << ";";

  // execute sql statement and get the result set
  result InventoryRes(N.exec(sql.str()));

  // first we need to check if the result set is empty
  if (InventoryRes.size() == 0) {
    return false;
  }

  // Then we need to get inventory item amount from result R
  int inventoryAmt = InventoryRes[0][0].as<int>();
  // get the version from the table and change it
  version = InventoryRes[0][1].as<int>();

  // we compare inventory amt and item amount
  if (inventoryAmt >= itemAmount) {
    return true;
  }
  else {
    return false;
  }
}

/*
   save order into the database,and set package into order member through reference.
*/
void saveOrderInDB(connection * C, Order & order) {
  //finally we need to save order in the order table
  work W(*C);
  stringstream sql;
  int addrx = order.getAddressX();
  int addry = order.getAddressY();
  int amount = order.getAmount();
  int upsid = order.getUPSId();
  int itemid = order.getItemId();
  int whid = order.getWhID();
  float item_price = order.getPrice();
  string customername = order.getCustomerName();
  float total_price = item_price * amount;
  sql << "INSERT INTO ORDERS (CUSTOMER_NAME, ADDR_X, ADDR_Y, AMOUNT, UPS_ID, ITEM_ID, PRICE, WH_ID) "
         "VALUES(" << W.quote(customername) << ", "
      << addrx << ", " << addry << ", " << amount << ", " << upsid << ", " << itemid
      << ", " << total_price << ", " << whid <<  ");SELECT currval('orders_pack_id_seq');";
  result orderRes = W.exec(sql.str());
  int packageId = orderRes[0][0].as<int>();
  order.setPackId(packageId);
  W.commit();
}

/*
    get description from the item 
*/
string getDescription(connection * C, int itemId) {
  //create nontransaction object for SELECT operation
  nontransaction N(*C);

  // create sql statement, we need to select item amount from inventory table
  stringstream sql;
  sql << "SELECT DESCRIPTION FROM ITEM WHERE "
         "ITEM_ID= "
      << itemId << ";";

  // execute sql statement and get the result set
  result ItemRes(N.exec(sql.str()));
  string description = ItemRes[0][0].as<string>();
  return description;
}

/*
    add inventory of item in the warehouse and update its version id.
    check if inventory exist this item, if not exist, we need to insert, else we need upadte
*/
void addInventory(connection * C, int whID, int count, int productId) {
  work W(*C);
  stringstream sql;
  sql << "INSERT INTO INVENTORY (ITEM_ID, ITEM_AMOUNT, WH_ID) "
         "VALUES("
      << productId << ", " << count << ", " << whID
      << ") ON CONFLICT (ITEM_ID, WH_ID) DO UPDATE "
         "set ITEM_AMOUNT = INVENTORY.ITEM_AMOUNT+"
      << count << ", VERSION = INVENTORY.VERSION+1"
      << " WHERE INVENTORY.ITEM_ID= " << productId << "AND INVENTORY.WH_ID= " << whID << ";";
  W.exec(sql.str());
  W.commit();
}

/*
    decrease inventory of the product in the warehouse and check the version id of the inventory.
    If version is not matched, throw exception.
*/
void decreaseInventory(connection * C, int whID, int count, int productId, int version) {
  work W(*C);
  stringstream sql;

  sql << "UPDATE INVENTORY set ITEM_AMOUNT = INVENTORY.ITEM_AMOUNT-" << count
      << ", VERSION = INVENTORY.VERSION+1"
      << " WHERE ITEM_ID= " << productId << " AND WH_ID= " << whID
      << " AND VERSION= " << version << ";";

  result Updates(W.exec(sql.str()));
  result::size_type rows = Updates.affected_rows();
  if (rows == 0) {
    throw MyException(
        "Invalid update: version of this order does not match.\n");
  }
  W.commit();
}

/*
    check whether the order status is packed
    if packed: return true, if not packed, return false.
*/
bool checkOrderPacked(connection * C, int packageID, int & whId) {
  work W(*C);
  stringstream sql;

  sql << "SELECT STATUS, WH_ID FROM ORDERS WHERE PACK_ID = " << packageID << ";";
  result statusRes(W.exec(sql.str()));
  string packageStatus = statusRes[0][0].as<string>();
  whId = statusRes[0][1].as<int>();
  W.commit();
  if (packageStatus == "packed") {
    return true;
  }
  else {
    return false;
  }
}

/*
    update specific order status to be 'packed'
*/
void updatePacked(connection * C, int packageId) {
  work W(*C);
  stringstream sql;
  string packed("packed");
  sql << "UPDATE ORDERS set STATUS= " << W.quote(packed)
      << " WHERE PACK_ID= " << packageId << ";";

  W.exec(sql.str());
  W.commit();
}

/*
    update specific order status to be 'delivered'
*/
void updateDelivered(connection * C, int packageId) {
  work W(*C);
  stringstream sql;
  string delivered("delivered");
  sql << "UPDATE ORDERS set STATUS= " << W.quote(delivered)
      << " WHERE PACK_ID= " << packageId << ";";

  W.exec(sql.str());
  W.commit();
}

/*
    update specific order status to be 'loaded'
*/
void updateLoaded(connection * C, int packageId) {
  work W(*C);
  stringstream sql;
  string loaded("loaded");
  sql << "UPDATE ORDERS set STATUS= " << W.quote(loaded)
      << " WHERE PACK_ID= " << packageId << ";";

  W.exec(sql.str());
  W.commit();
}

/*
    update specific order status to be 'loading'
*/
void updateLoading(connection * C, int packageId) {
  work W(*C);
  stringstream sql;
  string loaded("loading");
  sql << "UPDATE ORDERS set STATUS= " << W.quote(loaded)
      << " WHERE PACK_ID= " << packageId << ";";

  W.exec(sql.str());
  W.commit();
}

/*
    update specific order status to be 'delivering'
*/
void updateDelivering(connection * C, int packageId) {
  work W(*C);
  stringstream sql;
  string loaded("delivering");
  sql << "UPDATE ORDERS set STATUS= " << W.quote(loaded)
      << " WHERE PACK_ID= " << packageId << ";";

  W.exec(sql.str());
  W.commit();
}

