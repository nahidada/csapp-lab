
void main(){

    char *p = 0xf6962034;
    printf("p %p \n", p);

    p = p + 6096;
    printf("p %p \n", p);
    return;
}