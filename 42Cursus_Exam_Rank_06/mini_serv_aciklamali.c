#include <errno.h>       // Hata kontrolü için.
#include <string.h>      // String işlemleri için.
#include <unistd.h>      // write, close gibi sistem çağrıları için.
#include <netdb.h>       // Ağ adresleme için.
#include <sys/socket.h>  // Soket işlemleri için.
#include <netinet/in.h>  // İnternet protokolü için.
#include <sys/select.h>  // select() ve fd_set için.
#include <stdio.h>       // printf ve sprintf için.
#include <stdlib.h>      // malloc, calloc, free, exit için.

int client[4096] = {-1};          // Her istemci için ID saklar (-1 boş anlamına gelir).
char *messages[4096];             // Her istemci için mesaj buffer.
fd_set current, write_set, read_set; // select() için soket kümeleri.
int sockfd;                       // Sunucu soketi dosya tanıtıcısı.

void ft_error(char *str) {
    write(2, str, strlen(str)); // Hata mesajını stderr'e yazdırır.
    if (sockfd > 0)             // Sunucu soketi açıksa kapatır.
        close(sockfd);
    exit(1);                    // Programı sonlandırır.
}

void ft_send(int fd, int maxfd, char *str) {
    for (int i = maxfd; i > sockfd; i--) // Tüm istemcileri kontrol eder.
        if (client[i] != -1 && i != fd && FD_ISSET(i, &write_set)) 
            send(i, str, strlen(str), 0); // Mesajı yazmaya hazır istemcilere gönderir.
}

int extract_message(char **buf, char **msg) {
    char *newbuf;
    int i;
    *msg = 0; // Başlangıçta mesaj boş.
    if (*buf == 0) return (0); // Buffer boşsa çıkış yap.
    i = 0;
    while ((*buf)[i]) {
        if ((*buf)[i] == '\n') { // '\n' karakteri mesaj sonunu belirler.
            newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1)); // Kalan buffer için yer ayırır.
            if (newbuf == 0) return (-1); // Bellek hatası.
            strcpy(newbuf, *buf + i + 1); // Yeni buffer'a kalan veriyi kopyalar.
            *msg = *buf; // Tamamlanmış mesajı döndürür.
            (*msg)[i + 1] = 0; // Mesaj sonuna null-terminator ekler.
            *buf = newbuf; // Buffer güncellenir.
            return (1); // Mesaj başarıyla ayıklandı.
        }
        i++;
    }
    return (0); // Mesaj tamamlanmamış.
}

char *str_join(char *buf, char *add) {
    char *newbuf;
    int len;
    len = (buf == 0) ? 0 : strlen(buf); // Mevcut mesajın uzunluğunu hesaplar.
    newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1)); // Yeni buffer için bellek ayırır.
    if (newbuf == 0) return (0); // Bellek ayırma hatası.
    newbuf[0] = 0; // Yeni buffer'ı boş başlatır.
    if (buf != 0) strcat(newbuf, buf); // Mevcut mesajı kopyalar.
    strcat(newbuf, add); // Yeni gelen mesajı ekler.
    free(buf); // Eski buffer'ı serbest bırakır.
    return (newbuf); // Birleştirilmiş buffer'ı döndürür.
}

int main(int ac, char **av) {
    if (ac != 2) ft_error("Wrong number of arguments\n"); // Argüman kontrolü.

    sockfd = socket(AF_INET, SOCK_STREAM, 0); // Sunucu soketini oluşturur.
    if (sockfd == -1) ft_error("Fatal error\n"); // Hata durumunda programı sonlandırır.

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET; // IPv4 protokolü kullanır.
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 adresini bağlar.
    servaddr.sin_port = htons(atoi(av[1])); // Port numarasını ayarlar.
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
        ft_error("Fatal error\n"); // Bağlama hatası.
    if (listen(sockfd, 128) != 0)
        ft_error("Fatal error\n"); // Dinleme hatası.

    FD_ZERO(&current); // fd_set yapısını sıfırlar.
    FD_SET(sockfd, &current); // Sunucu soketini kümeye ekler.
    int maxfd = sockfd; // Başlangıç için maxfd sunucu soketi.
    int id = 0; // İstemciler için ID.

    while (1) {
        read_set = write_set = current; // Mevcut kümeleri okuma ve yazma setlerine kopyalar.
        if (select(maxfd + 1, &read_set, &write_set, NULL, NULL) < 0)
            continue; // select() hata verirse döngüye devam eder.
        if (FD_ISSET(sockfd, &read_set)) { // Yeni bağlantı kontrolü.
            int clientfd = accept(sockfd, NULL, NULL); // Yeni istemciyi kabul eder.
            if (clientfd < 0) continue; // Hata varsa devam et.
            FD_SET(clientfd, &current); // Yeni istemciyi kümeye ekler.
            char buf[64];
            sprintf(buf, "server: client %d just arrived\n", id); // Yeni istemci mesajı.
            client[clientfd] = id++; // İstemciye ID atar.
            messages[clientfd] = calloc(1, 1); // Mesaj buffer başlatılır.
            maxfd = (clientfd > maxfd) ? clientfd : maxfd; // maxfd'yi günceller.
            ft_send(clientfd, maxfd, buf); // Mesaj diğer istemcilere iletilir.
        }
        for (int fd = maxfd; fd > sockfd; fd--) { // Tüm istemcileri kontrol eder.
            if (FD_ISSET(fd, &read_set)) { // İstemciden gelen veri var mı?
                char buffer[4095];
                int bytes = recv(fd, buffer, 4094, 0); // Veri alır.
                if (bytes <= 0) { // Bağlantı kesildiyse.
                    FD_CLR(fd, &current); // fd_set'ten çıkar.
                    char buf[64];
                    sprintf(buf, "server: client %d just left\n", client[fd]); // Ayrılma mesajı.
                    ft_send(fd, maxfd, buf); // Ayrılma bilgisi diğer istemcilere iletilir.
                    client[fd] = -1; // İstemci ID'sini sıfırlar.
                    free(messages[fd]); // Mesaj buffer temizlenir.
                    close(fd); // İstemci soketi kapatılır.
                } else { // Mesaj geldiyse.
                    buffer[bytes] = 0; // Mesajın sonuna null-terminator ekler.
                    messages[fd] = str_join(messages[fd], buffer); // Mesajları birleştirir.
                    char *msg;
                    while (extract_message(&messages[fd], &msg)) { // Satır bazında mesaj ayıklar.
                        char buf[64 + strlen(msg)];
                        sprintf(buf, "client %d: %s", client[fd], msg); // Mesaj formatlanır.
                        ft_send(fd, maxfd, buf); // Mesaj diğer istemcilere iletilir.
                        free(msg); // Kullanılan mesaj belleğini serbest bırakır.
                    }
                }
            }
        }
    }
    return 0;
}
