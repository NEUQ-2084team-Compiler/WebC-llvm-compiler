# weblang-compiler

![build status](https://github.com/NEUQ-2084team-Compiler/weblang-compiler/actions/workflows/cmake.yml/badge.svg)

**weblang**编译器偏向于生成网络方面的执行代码，让小白开发者也可以做一个简单、易用、高性能、安全的http服务器。后期根据开发进度开发其他功能。

众所周知：C的http晦涩难学，安装第三方库过于繁琐,但性能最好;Python的http简单，但是是解释型语言，源码直接暴露;Java的http也有学习成本，
且针对简单的http服务使用Java这种复杂通用性语言有剩余空间，杀鸡焉用牛刀。

weblang编译器致力于打造一个简单语法的http服务器，上手即用语法简单，不用导任何包，不用管依赖问题， 生成的产物为编译好的可执行文件，文件小，安全，且能利用Python不能使用的多核cpu线程优势。

### 环境配置
* Debian系列系统运行项目目录下install.sh即可
* 其他操作系统需参照install.sh安装对应环境
  (环境安装过程中如遇无法自行解决的问题,请积极提issue😂)

### 一些简单的例子
helloserver.wbl
~~~
str hello() {
    ret 'hello from compiler';
}

int main() {
    echo('init...');
    str host = '0.0.0.0';
    int port = 9000;
    int core = 2;
    echo('get server...');
    int server_id = getServer(host, port, core);
    echo('server id is', server_id);
    addUrlHandler(server_id, 'GET', '/hello', 'text/html', hello);
    echo('start server in host', host, ',port is', port);
    startServer(server_id);
    ret 0;
}
~~~
sqltest.wbl
~~~
int main(){
    sqlite_connect_db('test.db');
    str res = sqlite_query_db('select * from person;');
    echo(res);
    ret 0;
}
~~~
qsort.wbl
~~~
int qsort(int a[],int left,int right){
    if (right <= left){
        ret -1;
    }
    int base = a[left];
    int j = left;
    int k = right;
    echo('begin wh');
    wh(j<k){
        wh(a[k]>=base && j<k){
            k = k - 1;
        }
        a[j] = a[k];
        wh(a[j]<=base && j<k){
            j = j + 1;
        }
        a[k] = a[j];
        echo('j,k,a[j],a[k]',j,k,a[j],a[k]);
    }
    a[k] = base;
    qsort(a,left,k);
    qsort(a,left,k- 1);
    ret 0;
}

int main(){
    int b[5]= {4,7,1,2,3};
    echo('original:',b[0],b[1],b[2],b[3],b[4]);
    int max = 4;
    echo('start qsort');
    qsort(b,0,max);
    echo('output qsort result');
    echo('now:',b[0],b[1],b[2],b[3],b[4]);
}
~~~
sleeptest.wbl
~~~
void sleep_1(){
    sleep(1);
}
void sleep_2(){
    sleep(2);
}
void sleep_3(){
    sleep(3);
}

int main(){
    sleep_1();
    sleep_2();
    sleep_3();
}
~~~
### 编译选项

* -i, --input <arg>   输入源文件

* -o, --output <arg>  输出目标文件

* -s, --as            生成可读汇编文件

* -h, --help <arg>    打印帮助手册

* -j, --object        只生成目标文件

* -t, --time_analysis 在函数内加入时间分析

### 目前支持程度

- 函数定义
  - 参数
- 二元表达式
  - &&、||
  - +、-、*、/、%、!=
- 函数调用
  - 传参
    - 基础类型、数组、多维数组、函数指针(fun_c<ret,...>)传参
- 条件分支关键字
  - if
- 循环关键字(c++ -> sysyplus)
  - for -> lp
  - While -> whl
  - break -> out
    - continue -> cont
- 函数返回 return -> ret
  - 保持单入单出特性
- 全局、局部变量
- 数组
  - 全局、局部数组/多维数组
  - 数组定义时初始化
- 字符串
  - 局部、全局声明、传值
- 语言特色
  - sleep(s) 休眠
  - echo(str) 输出
  - now(null) 获取当前秒数
- web功能
  - 创建socket
  - 连接url
  - 发送GET请求
  - 发送POST请求（正在开发中）
  - 支持TLSv1.3加密HTTPS（需要openssl支持，现默认使用TLSv1.2，兼容性好）
- 代码插桩/优化功能
  - 函数执行时间分析
  - 递归函数转非递归
- 生成AST语法树
- 生成对应系统架构的目标代码


### TODO list

- [x] 支持编译器内的函数指针
- [x] 支持HTTPS TLS v1.2加密，基于openssl
- [x] json数据的创建、修改
- [x] 支持连接mysql，执行sql语句返回json
- [x] 支持连接SQLite3