extern void print();

void fun(int* a, int* b, int jj){
    int x = 15;
    int y = 16;
    for (int i = 0; i < 10; i++){
        x++;
        a[i]=3;
    }

    
    for (int j = 0; j<10;j++){
        y++;
        b[j] = a[j];
    }
}

// just uncomment to show how the pass can identify invalid loops

//#define showfails
#ifdef showfails

void failadj(int* a, int* b, int jj){
    int x,y = 0;
    for (int i = 0; i < 10; i++){
        x++;
        a[i]=3;
    }

    print()

    for (int j = 0; j<10;j++){
        y++;
        b[j] = a[j];
    }
}

void failcfg(int* a, int* b, int jj){
    int x,y = 0;
    for (int i = 0; i < 10; i++){
        x++;
        a[i]=3;
    }

    if (jj) {
        return;
    }

    for (int j = 0; j<10;j++){
        y++;
        b[j] = a[j];
    }
}

void failiter(int* a, int* b, int jj){
    int x,y = 0;
    for (int i = 0; i < 10; i++){
        x++;
        a[i]=3;
    }

    

    for (int j = 0; j<5;j++){
        y++;
        b[j] = a[j];
    }
}

void failneg(int* a, int* b, int jj){
    int x,y = 0;
    for (int i = 0; i < 10; i++){
        x++;
        a[i]=3;
    }

    

    for (int j = 0; j<10;j++){
        y++;
        b[j] = a[j+3];
    }
}

#endif