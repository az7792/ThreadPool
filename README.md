### 测试

测试环境：Windows 11 专业版 23H2  16核 :AMD Ryzen 7 5800H with Radeon Graphics   3.20 GHz

#### 简单任务测试：

**任务：**

```c++
sum++;
```

**结果(第一行测试结果为非线程池版本)：**

|Threads|100 tasks|1000 tasks|10000 tasks|100000 tasks|1000000 tasks|10000000 tasks|
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
|1(noPool)|0ms|0ms|0ms|1ms|14ms|133ms|
|1|0ms|0ms|15ms|190ms|1919ms|22629ms|
|2|0ms|0ms|30ms|314ms|3154ms|31944ms|
|4|0ms|0ms|19ms|235ms|2369ms|23955ms|
|8|0ms|13ms|17ms|175ms|1851ms|18402ms|
|16|10ms|3ms|19ms|192ms|1918ms|19587ms|



#### 复杂任务测试：

**任务：**

```c++
 sum++;
 int count = 0;
 for (int num = 1; num <= 1000; ++num) {
      bool isPrime = true;
      for (int i = 2; i <= sqrt(num); ++i) {
           if (num % i == 0) {
                isPrime = false;
                break;
           }
      }
      if (isPrime) ++count;
 }
 return count;
```

**结果(第一行测试结果为非线程池版本)：**

|Threads|100 tasks|1000 tasks|10000 tasks|100000 tasks|1000000 tasks|
|:---:|:---:|:---:|:---:|:---:|:---:|
1(noPool)|4ms|41ms|485ms|4864ms|49809ms|
|1|8ms|55ms|495ms|5109ms|51289ms|
|2|0ms|31ms|315ms|2970ms|29187ms|
|4|0ms|14ms|156ms|1570ms|15521ms|
|8|0ms|13ms|78ms|810ms|8037ms|
|16|0ms|0ms|48ms|462ms|4757ms|