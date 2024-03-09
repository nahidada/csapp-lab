
void main(){

    char *p = 0xf69980c0;
    printf("p %p,  %d\n", p, p);

    p = p - 4;
    printf("p %p, %d \n", p, p);
    return;
}