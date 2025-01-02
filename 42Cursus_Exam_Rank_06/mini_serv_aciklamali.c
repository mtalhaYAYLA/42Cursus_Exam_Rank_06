#include <sys/select.h> // select() ve fd_set için gerekli
#include <stdio.h>      // sprintf() ve printf() için gerekli
#include <stdlib.h>     // malloc(), calloc(), free(), exit()
#include <string.h>     // strlen(), strcpy(), strcat()
#include <unistd.h>     // write(), close()
#include <netinet/in.h> // sockaddr_in, htons()

// Her istemci için ID ve mesajlarını saklamak için diziler
int client[4096] = {-1};        // Maksimum 4096 istemci için ID'ler
char *messages[4096];           // Her istemci için mesaj buffer
fd_set current, write_set, read_set; // Soket setleri
int sockfd;                     // Sunucu soket dosya tanıtıcısı

// Hata mesajlarını yazdırır ve programı sonlandırır
void ft_error(char *str) {
    write(2, str, strlen(str)); // stderr'e hata mesajı yaz
    if (sockfd > 0)             // Sunucu soketi açıksa kapat
        close(sockfd);
    exit(1);
}

// Mesajı belirli bir istemciye değil, diğer tüm istemcilere gönderir
void ft_send(int fd, int maxfd, char *str) {
    for (int i = maxfd; i > sockfd; i--) { // Tüm istemcileri tara
        if (client[i] != -1 && i != fd && FD_ISSET(i, &write_set)) {
            send(i, str, strlen(str), 0); // Mesajı gönder
        }
    }
}

// Mesajı buffer ile birleştirir
char *str_join(char *buf, char *add) {
    char *newbuf;
    int len = (buf == NULL) ? 0 : strlen(buf); // Eski buffer'ın uzunluğunu al
    newbuf = malloc(len + strlen(add) + 1);    // Yeni buffer için bellek ayır
    if (!newbuf)
        return NULL;
    newbuf[0] = '\0';             // Yeni buffer'ı boş başlat
    if (buf)
        strcat(newbuf, buf);      // Eski buffer'ı kopyala
    strcat(newbuf, add);          // Yeni mesajı ekle
    free(buf);                    // Eski buffer'ı serbest bırak
    return newbuf;
}

// Gelen buffer'dan bir mesaj çıkarır ve kalanını düzenler
int extract_message(char **buf, char **msg) {
    char *newline = strchr(*buf, '\n'); // '\n' karakterini bul
    if (!newline)
        return 0; // Mesaj tamamlanmamış
    *msg = strndup(*buf, newline - *buf + 1); // Satırı kopyala
    char *newbuf = strdup(newline + 1);      // Kalan buffer'ı kopyala
    free(*buf);                              // Eski buffer'ı serbest bırak
    *buf = newbuf;                           // Yeni buffer'a geçiş yap
    return 1;
}

int main(int ac, char **av) {
    if (ac != 2) // Argüman sayısını kontrol et
        ft_error("Wrong number of arguments\n");
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // TCP soketi oluştur
    if (sockfd == -1)
        ft_error("Fatal error\n");

    struct sockaddr_in servaddr = {0}; // Sunucu adresi ayarları
    servaddr.sin_family = AF_INET;      // IPv4 protokolü
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1
    servaddr.sin_port = htons(atoi(av[1])); // Port numarası

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
        ft_error("Fatal error\n");

    if (listen(sockfd, 128) != 0) // Dinleme başlat
        ft_error("Fatal error\n");

    FD_ZERO(&current);          // fd_set'i sıfırla
    FD_SET(sockfd, &current);   // Sunucu soketini ekle
    int maxfd = sockfd;         // Maksimum fd değeri
    int id = 0;                 // İlk istemci ID'si

    while (1) { // Sonsuz döngü
        read_set = write_set = current; // Okuma ve yazma setlerini güncelle
        if (select(maxfd + 1, &read_set, &write_set, NULL, NULL) < 0)
            continue; // select hatası varsa döngüye devam

        if (FD_ISSET(sockfd, &read_set)) { // Yeni bağlantı kontrolü
            int clientfd = accept(sockfd, NULL, NULL); // Bağlantıyı kabul et
            if (clientfd < 0)
                continue;
            FD_SET(clientfd, &current); // Yeni istemciyi ekle
            char buf[64];
            sprintf(buf, "server: client %d just arrived\n", id); // Giriş mesajı
            client[clientfd] = id++; // Yeni ID ata
            messages[clientfd] = calloc(1, 1); // Mesaj buffer başlat
            maxfd = (clientfd > maxfd) ? clientfd : maxfd; // maxfd güncelle
            ft_send(clientfd, maxfd, buf); // Mesajı diğer istemcilere gönder
        }

        for (int fd = maxfd; fd > sockfd; fd--) { // Tüm istemcileri tara
            if (FD_ISSET(fd, &read_set)) { // Veriler var mı kontrol et
                char buffer[4095];
                int bytes = recv(fd, buffer, 4094, 0); // Veri al
                if (bytes <= 0) { // Bağlantı kapandıysa
                    FD_CLR(fd, &current); // fd'yi çıkar
                    char buf[64];
                    sprintf(buf, "server: client %d just left\n", client[fd]);
                    ft_send(fd, maxfd, buf); // Çıkış mesajı gönder
                    client[fd] = -1;
                    free(messages[fd]); // Belleği serbest bırak
                    close(fd);
                } else { // Mesaj geldiyse
                    buffer[bytes] = 0;
                    messages[fd] = str_join(messages[fd], buffer); // Mesajı birleştir
                    char *msg;
                    while (extract_message(&messages[fd], &msg)) { // Satır satır işleme
                        char buf[64 + strlen(msg)];
                        sprintf(buf, "client %d: %s", client[fd], msg); // Mesaj formatı
                        ft_send(fd, maxfd, buf); // Mesajı diğer istemcilere gönder
                        free(msg); // Mesajı serbest bırak
                    }
                }
            }
        }
    }
    return 0;
}
