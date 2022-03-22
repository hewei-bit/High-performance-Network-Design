#include <openssl/aes.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <muduo/net/Buffer.h>

#include <stdio.h>

#include "timer.h"

muduo::net::Buffer clientOut, serverOut;

double now()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int bread(BIO *b, char *buf, int len)
{
  BIO_clear_retry_flags(b);
  muduo::net::Buffer* in = static_cast<muduo::net::Buffer*>(b->ptr);
  // printf("%s recv %d\n", in == &clientOut ? "server" : "client", len);
  if (in->readableBytes() > 0)
  {
    size_t n = std::min(in->readableBytes(), static_cast<size_t>(len));
    memcpy(buf, in->peek(), n);
    in->retrieve(n);

    /*
    if (n < len)
      printf("got %zd\n", n);
    else
      printf("\n");
      */
    return n;
  }
  else
  {
    //printf("got 0\n");
    BIO_set_retry_read(b);
    return -1;
  }
}

int bwrite(BIO *b, const char *buf, int len)
{
  BIO_clear_retry_flags(b);
  muduo::net::Buffer* out = static_cast<muduo::net::Buffer*>(b->ptr);
  // printf("%s send %d\n", out == &clientOut ? "client" : "server", len);
  out->append(buf, len);
  return len;
}

long bctrl(BIO *, int cmd, long num, void *)
{
  //printf("ctrl %d\n", cmd);
  switch (cmd) {
    case BIO_CTRL_FLUSH:
      return 1;
    default:
      return 0;
  }
}

int main(int argc, char* argv[])
{
  SSL_load_error_strings();
  // ERR_load_BIO_strings();
  SSL_library_init();
  OPENSSL_config(NULL);

  SSL_CTX* ctx = SSL_CTX_new(TLSv1_1_server_method());

  EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);
  if (argc > 3)
    SSL_CTX_set_tmp_ecdh(ctx, ecdh);
  EC_KEY_free(ecdh);

  const char* CertFile = argv[1];
  const char* KeyFile = argv[2];
  SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM);
  SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM);
  if (!SSL_CTX_check_private_key(ctx))
    abort();

  SSL_CTX* ctx_client = SSL_CTX_new(TLSv1_1_client_method());

  BIO_METHOD method;
  bzero(&method, sizeof method);
  method.bread = bread;
  method.bwrite = bwrite;
  method.ctrl = bctrl;
  BIO client, server;
  bzero(&client, sizeof client);
  bzero(&server, sizeof server);
  BIO_set(&client, &method);
  BIO_set(&server, &method);
  client.ptr = &clientOut;
  client.init = 1;
  server.ptr = &serverOut;
  server.init = 1;


  double start = now();
  const int N = 1000;
  SSL *ssl, *ssl_client;
  Timer tc, ts;
  for (int i = 0; i < N; ++i)
  {
    ssl = SSL_new (ctx);
    ssl_client = SSL_new (ctx_client);
    SSL_set_bio(ssl, &client, &server);
    SSL_set_bio(ssl_client, &server, &client);

    tc.start();
    int ret = SSL_connect(ssl_client);
    tc.stop();
    //printf("%d %d\n", ret, BIO_retry_type(&server));
    ts.start();
    int ret2 = SSL_accept(ssl);
    ts.stop();
    //printf("%d %d\n", ret2, BIO_retry_type(&client));

    while (true)
    {
      tc.start();
      ret = SSL_do_handshake(ssl_client);
      tc.stop();
      //printf("client handshake %d %d\n", ret, BIO_retry_type(&server));
      ts.start();
      ret2 = SSL_do_handshake(ssl);
      ts.stop();
      //printf("server handshake %d %d\n", ret2, BIO_retry_type(&client));
      //if (ret == -1 && BIO_retry_type(&server) == 0)
      //  break;
      //if (ret2 == -1 && BIO_retry_type(&client) == 0)
      //  break;
      if (ret == 1 && ret2 == 1)
        break;
    }

    //printf ("SSL connection using %s %s\n", SSL_get_version(ssl), SSL_get_cipher (ssl));
    if (i == 0)
      printf ("SSL connection using %s %s\n", SSL_get_version(ssl_client), SSL_get_cipher (ssl_client));
    //SSL_clear(ssl);
    //SSL_clear(ssl_client);
    if (i != N-1)
    {
      SSL_free (ssl);
      SSL_free (ssl_client);
    }
  }
  double elapsed = now() - start;
  printf("%.2fs %.1f handshakes/s\n", elapsed, N / elapsed);
  printf("client %.3f %.1f\n", tc.seconds(), N / tc.seconds());
  printf("server %.3f %.1f\n", ts.seconds(), N / ts.seconds());
  printf("server/client %.2f\n", ts.seconds() / tc.seconds());


  double start2 = now();
  const int M = 300;
  char buf[1024] = { 0 };
  for (int i = 0; i < M*1024; ++i)
  {
    int n = SSL_write(ssl_client, buf, sizeof buf);
    if (n < 0)
    {
      printf("%d\n", n);
    }
    clientOut.retrieveAll();
  }
  elapsed = now() - start2;
  printf("%.2f %.1f MiB/s\n", elapsed, M / elapsed);
  SSL_free (ssl);
  SSL_free (ssl_client);

  SSL_CTX_free (ctx);
  SSL_CTX_free (ctx_client);
}
