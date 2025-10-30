int globalint;
char globalchar;

main(){
    int localint;
    char localchar;
    input localint;
    localchar = 'c';
    if(localint == 1){
        localchar = 'd';
    }
    while(localint > 1){
        localchar = 'e';
        break;
    }
    localint = max( localint, 3) + 123;
    output localchar;
    globalint = localint;
    globalchar = localchar;
}

max(a, b){
    if(a<b) {return b;}
    else {return a;}
}
