#ifndef SYSYPLUS_COMPILER_KSQL_H
#define SYSYPLUS_COMPILER_KSQL_H

extern "C" {
/**
 * 连接到MySQL
 * @param hostname
 * @param username
 * @param password
 * @param schema
 * @param port 端口
 * @param charset 字符集
 * @return
 */
int _ksql_connect_db(const char *hostname, const char *username, const char *password, const char *schema, int port,
                     const char *charset);
/**
 * 释放内存
 * @return
 */
int _ksql_free_memory();
/**
 * 查询数据
 * @param sqlSentence select语句
 * @return
 */
const char *_ksql_query_db(const char *sqlSentence);
/**
 * 判断mysql是否连接
 * @return
 */
int _ksql_isMysqlConnected();
/**
 * 添加数据
 * @param sqlSentence select语句
 * @return
 */
int _ksql_exec_db(const char *sqlSentence);
/**
 * 连接到SQLite3
 * @param path
 * @return
 */
int _ksqlite_connect_db(const char *path);
/**
 * 执行SQLite3语句
 * @param sentence
 * @return
 */
int _ksqlite_exec_db(const char *sentence);
/**
 * 执行SQLite3语句(不返回数据)
 * @param sentence
 * @return
 */
const char *_ksqlite_query_db(const char *sentence);
}

#endif //SYSYPLUS_COMPILER_KSQL_H
