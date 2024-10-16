#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "tosfs.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <filesystem image file>\n", argv[0]);
        return 1;
    }

    // Ouvrir le fichier de l'image du système de fichiers
    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    // Utilisation de mmap pour mapper le fichier dans la mémoire
    void *mapped_fs = mmap(NULL, TOSFS_BLOCK_SIZE * 32, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped_fs == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    // Récupérer le superblock à l'adresse mappée
    struct tosfs_superblock *sb = (struct tosfs_superblock *) mapped_fs;

    // Vérifier le numéro magique
    if (sb->magic != TOSFS_MAGIC) {
        fprintf(stderr, "Invalid filesystem magic number: 0x%x\n", sb->magic);
        munmap(mapped_fs, TOSFS_BLOCK_SIZE * 32);
        close(fd);
        return 1;
    }

    // Afficher les informations sur le superblock
    printf("Filesystem Information:\n");
    printf("Magic number: 0x%x\n", sb->magic);
    printf("Block bitmap: 0x%x\n", sb->block_bitmap);
    printf("Inode bitmap: 0x%x\n", sb->inode_bitmap);
    printf("Block size: %u\n", sb->block_size);
    printf("Total blocks: %u\n", sb->blocks);
    printf("Total inodes: %u\n", sb->inodes);
    printf("Root inode: %u\n", sb->root_inode);

    // Nettoyage
    munmap(mapped_fs, TOSFS_BLOCK_SIZE * 32);
    close(fd);

    return 0;
}
