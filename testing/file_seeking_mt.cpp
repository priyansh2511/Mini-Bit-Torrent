#include <bits/stdc++.h>
using namespace std;
struct x
{
    int start;
    int end;
} ThreadInfo;
void *fstream_file(void *)
{
    int block_size = 32;
    fstream file, file2;
    file.open("filetest.txt", ios::in | ios::binary);
    file2.open("newfiletext.txt", ios::in | ios::out | ios::trunc | ios::binary);
    file.seekg(0, ios::end);
    int pos = file.tellg();
    file.seekg(0, ios::beg);
    int size = pos;
    int no_of_blocks = size / block_size;
    int last_size = size % block_size;
    cout << pos << endl;
    for (int i = 0; i <= no_of_blocks; i++)
    {
        char *buffer = new char[block_size];
        file.read(buffer, block_size);
        string x(buffer);
        int y = file.gcount();
        cout << y << endl;
        file2.write(buffer, y);
    }
    char *buffer = new char[last_size];
    file.read(buffer, last_size);
    string x(buffer);
    int y = file.gcount();
    cout << y << endl;
    file2.write(buffer, y);
    pos = file.tellg();
    cout << pos << endl;
    file.close();
    file2.close();
}
void fb()
{
    int block_size = 32;
    int size = 1024;
    fstream file;
    file.open("newfile.txt", ios::out | ios::binary | ios::trunc);
    file.seekp(size - 1, ios::end);
    file.put('\0');
    int pos = file.tellp();
    file.close();
    cout << pos << endl;
}
void ft()
{
    fstream file, file2;
    file.open("filetest.txt", ios::in | ios::binary | ios::out);
    file2.open("filetest.txt", ios::in | ios::binary | ios::out);
    file.seekp(15, ios::beg);
    file2.seekp(20, ios::beg);
    file.put('@');
    file2.put('#');
    cout << file.tellg() << endl;
    cout << file2.tellg() << endl;
    file.close();
    file2.close();
}
int main()
{
    pthread_t worker1, worker2;
    //fstream_file();
    //fb();
    ft();
    cout << "hello";
    string temp_s = "upload_file"+ ' ';
    return 0;
}