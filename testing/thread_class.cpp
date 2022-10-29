#include <bits/stdc++.h>
#include <pthread.h>
using namespace std;
class abc
{
public:
    int a;
    string c;
};
typedef struct new_struct
{
    abc i;
    vector<string> s;
} cdf;
void *startup(void *arg)
{
    cdf x = *((cdf *)arg);
    cout << "G" << endl;
}
int main()
{
    cdf *new_c = (cdf *)malloc(sizeof(cdf));
    ;
    new_c->i.a = 23;
    new_c->i.c = "asdas";
    new_c->s = {"hello", "world"};
    pthread_t worker;
    pthread_create(&worker, NULL, startup, new_c);
    pthread_join(worker, NULL);
    return 0;
}