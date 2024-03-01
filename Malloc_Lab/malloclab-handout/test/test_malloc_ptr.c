#include <stdio.h>
#include <stdlib.h>

int main() {
    // 使用 malloc 分配足够的空间来存储一个指针
    int* ptr = (int*)malloc(sizeof(int*)*2);

    if (ptr == NULL) {
        // 检查内存分配是否成功
        printf("内存分配失败\n");
        return 1; // 返回错误码
    }

    // 假设有一个地址需要存储
    int x = 222;
    // 将地址存储到分配的内存中
    *ptr = &x;

    // 打印存储的地址值
    printf("存储的地址值：%p\n", *ptr);

    int * p = *ptr;
    printf("val: %d \n", *p);

    printf("存储的地址值：%p\n", &x);

    *(ptr+1) = 333;
    printf("val: %d \n", *(ptr+1));

    // 释放分配的内存
    free(ptr);

    return 0; // 返回成功码
}