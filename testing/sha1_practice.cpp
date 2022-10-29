#include <bits/stdc++.h>
#include <openssl/sha.h>
using namespace std;
string generate_SHA1(const char *target, int size)
{
    unsigned char temp[SHA_DIGEST_LENGTH];
    char buffer[SHA_DIGEST_LENGTH * 2];
    memset(temp, 0x0, SHA_DIGEST_LENGTH);
    memset(buffer, 0x0, SHA_DIGEST_LENGTH * 2);
    SHA1((unsigned char *)target, size, temp);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sprintf((char *)&(buffer[i * 2]), "%02x", temp[i]);
    }
    string temp_hash(buffer);
    return temp_hash;
}
string generate_SHA1(string message)
{
    return generate_SHA1(message.c_str(), message.size());
}
int main()
{

    int i = 0;
    unsigned char temp[SHA_DIGEST_LENGTH];
    char buf[SHA_DIGEST_LENGTH * 2];
    string s;
    cin >> s;
    memset(buf, 0x0, SHA_DIGEST_LENGTH * 2);
    memset(temp, 0x0, SHA_DIGEST_LENGTH);

    SHA1((unsigned char *)s.c_str(), s.size(), temp);

    for (i = 0; i < SHA_DIGEST_LENGTH; i++)
    {
        sprintf((char *)&(buf[i * 2]), "%02x", temp[i]);
    }

    printf("SHA1 of %s is %s\n", s.c_str(), buf);
    cout << generate_SHA1(s).size() << endl;
    return 0;
}
