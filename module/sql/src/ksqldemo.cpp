//
// Created by 金韬 on 2021/3/20.
//
#include <iostream>

#include "ksql.h"


int main() {
    _ksql_connect_db("127.0.0.1", "root", "123456", "school", 3306, "utf8");
    _ksql_exec_db("INSERT INTO category(categoryid,pid,categoryname) VALUES(9,2,'电子信息')");
    auto a = _ksql_query_db("select * from category");
    std::cout << a;

    // std::cout << _ksql_query_db("select * from weather");
    // std::cout << _ksql_insert_db("insert into `weather` (`area`,`temp`) values('哈','30')");
    // std::cout<< _ksql_update_db("update `weather` set `area` = '石家庄' where `temp`='30' ");
    //std::cout<< _ksql_delete_db("delete from `weather` where `area`='哈'");
}