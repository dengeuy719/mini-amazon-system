#include "sqlOperation.h"




//when reveive a order_id from the web-page, it will query the packages included in the order
std::vector<std::string> getPackagesfOrder(connection * C, int order_id)
{
    nontransaction N(*C);
    stringstream sql;


    sql << "SELECT PACKAGE_ID FROM PACKAGE WHERE "
         "ORDER_ID_ID= " << order_id << ";";


    // execute sql statement and get the result set
    result package_result(N.exec(sql.str()));
   

    // Convert the result set to a list of strings
    std::vector<std::string> packages;
    for (auto row = package_result.begin(); row != package_result.end(); ++row) {
        for (auto col = row->begin(); col != row->end(); ++col) {
            packages.push_back(col->as<std::string>());
        }
    }       

    return packages;

    
}


//one package means one purchase action,it will change the status of the package to packing and set its wh_id
//you can use updatepackWhID and updatepackPacking to complete this fucntion individually
void purchaseProduct(connection * C, int package_id,int wh_id)
{

    work W(*C);
    stringstream sql;


    sql << "UPDATE PACKAGE SET STATUS= " << W.quote("packing") << ", WH_ID= " 
        << wh_id << " WHERE PACKAGE_ID= "<< package_id << ";";

    W.exec(sql.str());
    W.commit();

}




int getOrderAddrx(connection* C, int order_id)
{
    nontransaction N(*C);
    stringstream sql;


    sql << "SELECT ORDER_ADDR_X FROM \"ORDER\" WHERE "
        "ORDER_ID= " << order_id << ";";


    // execute sql statement and get the result set
    result order_x_result(N.exec(sql.str()));


    // Then we need to get inventory item amount from result R
    int order_x = order_x_result[0][0].as<int>();
    return order_x;

}
int getOrderAddry(connection* C, int order_id)
{

    nontransaction N(*C);
    stringstream sql;


    sql << "SELECT ORDER_ADDR_Y FROM \"ORDER\" WHERE "
        "ORDER_ID= " << order_id << ";";


    // execute sql statement and get the result set
    result order_y_result(N.exec(sql.str()));


    // Then we need to get inventory item amount from result R
    int order_y = order_y_result[0][0].as<int>();
    return order_y;


}


vector<int> getPackageIDs(connection* C, int order_id) {
    vector<int> package_ids;
    nontransaction N(*C);

    stringstream sql;
    sql << "SELECT PACKAGE_ID FROM PACKAGE WHERE ORDER_ID = " << order_id << ";";

    result R(N.exec(sql.str()));

    for (auto row : R) {
        int package_id;
        row[0].to(package_id);
        package_ids.push_back(package_id);
    }

    return package_ids;
}


int getPackageProductID(connection* C, int package_id)
{
    nontransaction N(*C);
    stringstream sql;


    sql << "SELECT PRODUCT_ID FROM PACKAGE WHERE "
        "PACKAGE_ID= " << package_id << ";";


    // execute sql statement and get the result set
    result product_id_result(N.exec(sql.str()));


    // Then we need to get inventory item amount from result R
    int product_id = product_id_result[0][0].as<int>();

    return product_id;


}



string getPackageProductDesc(connection* C, int package_id)
{
    nontransaction N(*C);
    stringstream sql,sql_new;


    sql << "SELECT PRODUCT_ID FROM PACKAGE WHERE "
        "PACKAGE_ID= " << package_id << ";";

    // execute sql statement and get the result set
    result product_id_result(N.exec(sql.str()));

    // Then we need to get inventory item amount from result R
    int product_id = product_id_result[0][0].as<int>();

    sql_new << "SELECT PRODUCT_DESC FROM PRODUCT WHERE "
        "PRODUCT_ID= " << product_id << ";";

    // execute sql statement and get the result set
    result product_desc_result(N.exec(sql_new.str()));
    string product_desc = product_desc_result[0][0].as<string>();

    return product_desc;


}


int getPackageOrderID(connection* C, int package_id) {
    nontransaction N(*C);

    stringstream sql;
    sql << "SELECT ORDER_ID FROM PACKAGE WHERE PACKAGE_ID = " << package_id << ";";

    result R(N.exec(sql.str()));

    int order_id = -1;
    if (!R.empty()) {
        R[0][0].to(order_id);
    }

    return order_id;
}


int getPackageUpsID(connection* C, int package_id) {
    nontransaction N(*C);

    stringstream sql;
    sql << "SELECT O.UPS_ID FROM PACKAGE P "
           "JOIN \"ORDER\" O ON P.ORDER_ID = O.ORDER_ID "
           "WHERE P.PACKAGE_ID = " << package_id << ";";

    result R(N.exec(sql.str()));

    int ups_id = -1;
    if (!R.empty()) {
        R[0][0].to(ups_id);
    }

    return ups_id;
}


string getPackageDesc(connection* C, int package_id)
{
    nontransaction N(*C);
    stringstream sql;


    sql << "SELECT PACKAGE_DESC FROM PACKAGE WHERE "
        "PACKAGE_ID= " << package_id << ";";


    // execute sql statement and get the result set
    result package_desc_result(N.exec(sql.str()));


    // Then we need to get inventory item amount from result R
    string package_desc = package_desc_result[0][0].as<string>();
    return package_desc;



}




int getPackageAmount(connection* C, int package_id)
{
    nontransaction N(*C);
    stringstream sql;


    sql << "SELECT AMOUNT FROM PACKAGE WHERE "
        "PACKAGE_ID= " << package_id << ";";


    // execute sql statement and get the result set
    result package_amount_result(N.exec(sql.str()));


    // Then we need to get inventory item amount from result R
    int package_amount = package_amount_result[0][0].as<int>();
    return package_amount;



}

void updateOrderAddr(connection* C, int orderID, int new_x, int new_y) {
    work W(*C);
    stringstream sql;
    sql << "UPDATE \"ORDER\" SET ORDER_ADDR_X = " << new_x
        << ", ORDER_ADDR_Y = " << new_y
        << " WHERE ORDER_ID = " << orderID << ";";

    W.exec(sql.str());
    W.commit();
} 
vector<int> getPackedPackageIDs(connection* C, int wh_id) {
    vector<int> package_ids;
    nontransaction N(*C);

    stringstream sql;
    sql << "SELECT PACKAGE_ID FROM PACKAGE WHERE STATUS = 'packed' AND WH_ID = " << wh_id << ";";

    result R(N.exec(sql.str()));

    for (auto row : R) {
        int package_id;
        row[0].to(package_id);
        package_ids.push_back(package_id);
    }

    return package_ids;
}

void setPackagesWhID(connection* C, int orderID, int whID)
{

    work W(*C);
    stringstream sql;
    sql << "UPDATE PACKAGE SET WH_ID= " << whID
        << " WHERE ORDER_ID= " << orderID << ";";

    W.exec(sql.str());
    W.commit();

}


void updatepackPacking(connection* C, int package_id)
{

    work W(*C);
    stringstream sql;


    sql << "UPDATE PACKAGE SET STATUS =  'packing' "
        << " WHERE PACKAGE_ID= " << package_id << ";";

    W.exec(sql.str());
    W.commit();

}


//update the status of package into packed
void updatepackPacked(connection * C, int package_id)
{

    work W(*C);
    stringstream sql;


    sql << "UPDATE PACKAGE SET STATUS=  'packed'" 
         << " WHERE PACKAGE_ID= "<< package_id << ";";

    W.exec(sql.str());
    W.commit();

}

//update the status of package into packed
void updatepackLoading(connection * C, int package_id)
{

    work W(*C);
    stringstream sql;


    sql << "UPDATE PACKAGE SET STATUS= 'loading'" 
         << " WHERE PACKAGE_ID= "<< package_id << ";";

    W.exec(sql.str());
    W.commit();

}

//update the status of package into packed
void updatepackLoaded(connection * C, int package_id)
{

    work W(*C);
    stringstream sql;


    sql << "UPDATE PACKAGE SET STATUS= 'loaded' "
         << " WHERE PACKAGE_ID= "<< package_id << ";";

    W.exec(sql.str());
    W.commit();

}

//update the status of package into packed
void updatepackDelivering(connection * C, int package_id)
{

    work W(*C);
    stringstream sql;


    sql << "UPDATE PACKAGE SET STATUS= 'delivering' " 
         << " WHERE PACKAGE_ID= "<< package_id << ";";

    W.exec(sql.str());
    W.commit();

}

//update the status of package into packed
void updatepackDelivered(connection * C, int package_id)
{

    work W(*C);
    stringstream sql;


    sql << "UPDATE PACKAGE SET STATUS= 'delivered' " 
         << " WHERE PACKAGE_ID= "<< package_id << ";";

    W.exec(sql.str());
    W.commit();

}



