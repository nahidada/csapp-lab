#include <stdio.h>
#include <stdlib.h>

int main() {
    // 使用 malloc 分配足够的空间来存储一个指针
    int* ptr;

    // 假设有一个地址需要存储
    int x = 222;
    // 将地址存储到分配的内存中
    *ptr = &x;

    // 打印存储的地址值
    printf("存储的地址值：%p\n", *ptr);

    int * p = *ptr;
    printf("val: %d \n", *p);

    *(ptr) = 333;
    printf("val: %d \n", *(ptr));

    return 0; // 返回成功码
}