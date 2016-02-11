#ifndef SSLENCODER_H
#define SSLENCODER_H

#include <QObject>
#include <QByteArray>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/objects.h>

class SSLEncoder : public QObject
{
    Q_OBJECT
public:
    explicit SSLEncoder(QObject *parent = 0);

    static QByteArray encryptRSA(QByteArray data, QByteArray keyArray);
    static QByteArray compressData(const QByteArray &data);
    static quint32 CRC32(const QByteArray& data);


    static QByteArray encodeAES256(QByteArray data, bool toBase64, bool isText);
    static QByteArray decodeAES256(QByteArray data, bool fromBase64);
    static void handleErrors();


signals:

public slots:
protected:

    static QByteArray decodeAES(QByteArray data, QString password);
    static QByteArray sha256(const QByteArray &text);
    static quint32 updateCRC32(unsigned char ch, quint32 crc);
    static int decryptAES256(unsigned char *ciphertext, int ciphertext_len, unsigned char *key, unsigned char *iv, unsigned char *plaintext);
    static int encryptAES256(unsigned char *plaintext, int plaintext_len, unsigned char *key, unsigned char *iv, unsigned char *ciphertext);
};

#endif // SSLENCODER_H

