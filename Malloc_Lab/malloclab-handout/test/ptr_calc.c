
void main(){

    char *p = 0xf;
    printf("p %p,  %d\n", p, p);

    p = p -1;
    printf("p %p, %d \n", p, p);
    return;
}