#include <stdio.h>

int main() {
    FILE *file;
    char operation;
    long long address;
    int size;

    // 打开文件
    file = fopen("/home/csapp/Cache_Lab/cachelab-handout/traces/dave.trace", "r");
    if (file == NULL) {
        printf("无法打开文件\n");
        return 1;
    }

    // 从文件中读取数据
    fscanf(file, " %c %llx,%d", &operation, &address, &size);

    // 输出读取到的数据
    printf("操作：%c\n", operation);
    printf("地址：%llx\n", address);
    printf("地址 >> 2 ：%llx\n", address >> 2);
    printf("大小：%d\n", size);

    // 关闭文件
    fclose(file);

    return 0;
}