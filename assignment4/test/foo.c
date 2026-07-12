void fun(int* a, int* b){
    int x,y = 0;
    for (int i = 0; i < 10; i++){
        x++;
        a[i]=3;
    }
    for (int j = 0; j<10;j++){
        y++;
        b[j] = a[j+1];
    }
}