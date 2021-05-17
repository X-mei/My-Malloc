## Analysis of result
### Organization of Report 
The report is divided into three main sections, where I discuss the design process as well as some unique implementation detials in the First Section, the result I acquired as well as my analysis towards it in the Second Section, and comparison to the glibc version of malloc & source code analysis of it. 
### 1. Implementation Overview
#### 1.1 Locking version Implementation
The locking version is reletively straight forward. The core datastructure in the design of this malloc library is the free list. To make it thread safe, everytime a modification is made to the free list should be protected by a lock. Since most function involes altering the list, namely insert/merge/remove list, my design is to put lock right outside the interface method `myMalloc()`.
```C
void * ts_malloc_lock(size_t size){
  assert(pthread_mutex_lock(&lock) == 0);
  void * ptr = myMalloc(size, sbrk, &head_norm, &tail_norm);//was myMalloc(size);
  assert(pthread_mutex_unlock(&lock) == 0);
  return ptr;
}
```
For `free()`, I also have a interface method `myFree()` to handle free in both versions.
```C
void ts_free_lock(void * ptr){
  assert(pthread_mutex_lock(&lock) == 0);
  myFree(ptr, &head_norm, &tail_norm);//was myFree(ptr);
  assert(pthread_mutex_unlock(&lock) == 0);
}
```
#### 1.2 No-lock version Implementation
No lock version utilize Thread-Local Storage, which means each thread will have a seperate free list. This would eliminate most of our need to lock the body of malloc. The only portion left unprotected would be `sbrk()`system call so we simply lock it and it should be thread safe.

To make the free list thred safe, a very simple solution woule be to make free list's head and tail to a thread-local variable since they are foundation of a free list. 

After figuring this out, I realized that both version shouldn't share the same head or tail variable since they are of different type. One being `block*` and one being `__thread block*`. That said, make the version 1 free list thread-local woule make the lock superflous and possibly lead to significant performance degeneration. Consequntly, I refactored the code to make two pairs of head/tail for two version and altered the mothod to take in two input of type `block**`(if applicable) so I dont't have to duplicate code for two versions.
```C
//Normal linked list variable for lock version
block * head_norm = NULL;
block * tail_norm = NULL;
//Linked list using Thread Local Storage for no lock version
__thread block * head_tls = NULL;
__thread block * tail_tls = NULL;
```
The `malloc()` and `free()` are pretty simple since we don't have to worry about locks.
```C
void ts_free_lock(void * ptr){
  assert(pthread_mutex_lock(&lock) == 0);
  myFree(ptr, &head_norm, &tail_norm);
  assert(pthread_mutex_unlock(&lock) == 0);
}

void ts_free_nolock(void * ptr){
  myFree(ptr, &head_tls, &tail_tls);
}
```
#### 1.3 Other Non-critical Sections
Other than the changes mentioned above, all the other portion of the code is left unchanged. Since part2 only consider best-fit, first-fit function is removed and corresponding branch in `myMalloc()` is removed.
### 2 Result and Analysis
I runned the test 10 times for each version. The result is as follows:
|Version|Locking|No Lock|
|:-|:-|:-|
|Average Time Took|1.478|0.303|
|Maximum Time Took|1.526|0.314|
|Minimum Time Took|1.429|0.296|
|Average Segment Size(Bytes)|42470634|42515722|
|Maximum Segment Size(Bytes)|42471920|42534134|
|Minimum Segment Size(Bytes)|42468012|42496432|
#### 2.1 Result Analysis
Based on the result, no lock version is five times faster than locking version. This significant speed up is due to the fact that we are no locking all portion of the code but rather having each thread operate on different free list so they can happen concurrently. As for locking version, all threads share one singly free list, a reletively lone free list would make it harder to find the best fit free block. 

Based on the results, there is no evident difference between two versions on the data segment size. Since the load are expected to distribute to different thread and thread count is the same for both version. We can imagine that version1's free list can be formed by connecting all free list from version2, which, should have similar data segment size performance.

### 3 Glibc version of malloc/free(used in Most Opreating System)
To further explore this topic, I decided to look into the source code of the malloc/free library used to do memory management in real operating system. I started by running some test using this particular malloc/free. It turns out the result is much better than I expected:
|Version|Locking|No Lock|Glibc|
|:-|:-|:-|:-|
|Average Time Took|1.478|0.303|0.027|
|Average Segment Size(Bytes)|42470634|42515722|135168|

At that time my understanding of the standard malloc/free library is that it do not differ much from our implementation. That is, we all used a list sturcture to manage the memory, used some traversing algorithm to find the fitting block to assign. The only two difference that I could think of is the actual malloc uses `mmap` instead of `sbrk` when the size required exceed certain threshold and maybe some difference in how the thread-safe portion is implemented. But it turns out the difference is actually huge.

According to Doug Lea, the person who wrote this version of malloc/free. There are several goals to meet when designing a good memory management library:
__1. Maximizing Compatibility:__

An allocator should be plug-compatible with others; in particular it should obey ANSI/POSIX conventions.


__2. Maximizing Portability:__

Reliance on as few system-dependent features (such as system calls) as possible, while still providing optional support for other useful features found only on some systems; conformance to all known system constraints on alignment and addressing rules.


__3. Minimizing Space:__

The allocator should not waste space: It should obtain as little memory from the system as possible, and should maintain memory in ways that minimize fragmentation -- ``holes''in contiguous chunks of memory that are not used by the program.

__4. Minimizing Time:__

The malloc(), free() and realloc routines should be as fast as possible in the average case.

__5. Maximizing Tunability:__
Optional features and behavior should be controllable by users either statically (via #define and the like) or dynamically (via control commands such as mallopt).

__6. Maximizing Locality:__

Allocating chunks of memory that are typically used together near each other. This helps minimize page and cache misses during program execution.

__7. Maximizing Error Detection:__

It does not seem possible for a general-purpose allocator to also serve as general-purpose memory error testing tool such as Purify. However, allocators should provide some means for detecting corruption due to overwriting memory, multiple frees, and so on.

__8. Minimizing Anomalies:__

An allocator configured using default settings should perform well across a wide range of real loads that depend heavily on dynamic allocation -- windowing toolkits, GUI applications, compilers, interpretors, development tools, network (packet)-intensive programs, graphics-intensive packages, web browsers, string-processing applications, and so on.