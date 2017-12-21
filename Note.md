### 这是阅读本项目时，需要的基础知识
1. 单引号引起的字符其实代表一个整数，双引号里面其实是代表指向无名数组起始字符的指针
2. 用单引号引起的一个字符大小就是一个字节;而用双引号引起的字符串大小是字符的总大小+1，因为用双引号引起的字符串会在字符串末尾添加一个二进制为0的字符'\0'。
```c
	char w[100][100];
	if(w[100][0] == "*")	// 报错，c禁止指针和整数比较
	if(w[100][0] == '*')	// 这是正确的
```
3. strlen所作的仅仅是一个计数器的工作，它从内存的某个位置（可以是字符串开头，中间某个位置，甚至是某个不确定的内存区域）开始扫描，直到碰到第一个字符串结束符'\0'为止，然后返回计数器值(长度不包含'\0’)；
4. static的作用，https://www.cnblogs.com/dc10101/archive/2007/08/22/865556.html
5. atoi() 函数用来将字符串转换成整数(int)，其原型为：int atoi (const char * str);【函数说明】atoi() 函数会扫描参数 str 字符串，跳过前面的空白字符（例如空格，tab缩进等，可以通过 isspace() 函数来检测）p，直到遇上数字或正负符号才开始做转换，而再遇到非数字或字符串结束时('\0')才结束转换，并将结果返回。 http://www.cnblogs.com/A-FM/p/5018168.html
6. 关于AF_INET 和PF_INEThttp://www.cnblogs.com/zhangjing0502/archive/2012/06/26/2563727.html
7. sizeof 是运算符， strlen是函数， http://www.cnblogs.com/ttltry-air/archive/2012/08/30/2663366.html
8. dup2: http://blog.csdn.net/life_liver/article/details/8554095
9. pipe: http://blog.51cto.com/huyoung/436494
11. fork: http://blog.csdn.net/jason314/article/details/5640969
12. excel: http://www.cnblogs.com/mickole/p/3187409.html
13. setenv: http://www.cnblogs.com/frank-yxs/p/5925838.html
14. hotel: http://blog.csdn.net/yaxiya/article/details/6722083 把本机字节顺序转化为网络字节顺序
15. backlog: http://zdyi.com/books/unp/s2/2.5.2.html
