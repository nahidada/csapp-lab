#include <stddef.h>
#include <stdio.h>

#define DSIZE 8

void printBinary(size_t num) {
    // 获取 size_t 类型的二进制位数
    size_t bits = sizeof(size_t) * 8;
    int i;
    
    // 从最高位开始，逐位打印
    for (i = bits - 1; i >= 0; i--) {
        // 右移 i 位，检查当前位是否为 1
        if (num & (1 << i))
            printf("1");
        else
            printf("0");
        
        // 在每 8 位之后添加一个空格，便于观察
        if (i % 8 == 0)
            printf(" ");
    }
    printf("\n");
}

size_t cal2(size_t size){
    size_t target_size;
    target_size = size <= DSIZE ? 2*DSIZE : DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    return target_size;
}

size_t cal1(size_t size){
    size_t target_size;

    printf("%d \n",  (size + DSIZE + (DSIZE -1)) );
    printBinary( (size + DSIZE + (DSIZE -1)) );

    printf("%d\n", ((size + DSIZE + (DSIZE -1))) && ~(0xF) );
    printBinary( ((size + DSIZE + (DSIZE -1))) && ~(0xF) );
    
    target_size = (size <= DSIZE ? 2*DSIZE : ((size + DSIZE + (DSIZE -1))) && ~(0xF));

    
    return target_size;
}

int main(){
    size_t size = 13;

    size_t t_size;
    t_size = cal1(size);

    printf("cal1 t_size %d \n",t_size);

    t_size = cal2(size);
    printf("cal2 t_size %d \n", t_size);

    return 0;
}