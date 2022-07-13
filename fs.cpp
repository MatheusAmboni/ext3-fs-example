#include "fs.h"
#include <cstring>
#include <cmath>
#include <bitset>

void initFs(std::string fsFileName, int blockSize, int numBlocks, int numInodes){
    //open file
    FILE *arq = fopen(fsFileName.c_str(), "w+");

    int mapsize = (numBlocks -1)/8; //without root
    char bytezero = 0x00; 
    char firstbyte = 0x01;

    //caractheristics of fs
    fwrite(&blockSize, sizeof (char), 1, arq);
    fwrite(&numBlocks, sizeof (char), 1, arq);
    fwrite(&numInodes, sizeof (char), 1, arq);

    //map
    fwrite(&firstbyte, sizeof (char), 1, arq);
    for (int i = 0; i < mapsize; i++){
    fwrite(&bytezero, sizeof (char), 1, arq);
    }
    
    //inode dir root
    INODE root = {1,1, "/", 0, {0,0,0},{0,0,0},{0,0,0}}; 
    fwrite(&root, sizeof(root), 1, arq);

    //inodes
    INODE zero = {0,0,0,0,0,0,0};
    for(int i=0; i < numInodes - 1; i++){ 
        fwrite(&zero, sizeof(zero), 1, arq);
    }

    //inode of /
    fwrite(&bytezero, sizeof (char), 1, arq); 
    
    //blocks
    for(int i=0; i < numBlocks * blockSize; i++){ 
        fwrite(&bytezero, sizeof (char), 1, arq);
    }
    fclose(arq);
}

void addFile(std::string fsFileName, std::string filePath, std::string fileContent){
    FILE *arq = fopen(fsFileName.c_str(), "r+");

    //get fs information 
    char information_file_system[3];
    fread(information_file_system, sizeof(char), 3, arq);

    //check free inode to store file
    int freeInode;
    int mapsize = ceil((float)information_file_system[1]/8.0);  
    int inodeStart = 3 + mapsize;
    for (int i = 0; i < information_file_system[2]; i++){
        char is_used;
        fseek(arq, inodeStart + i * sizeof(INODE), SEEK_SET);
        fread(&is_used, sizeof(unsigned char), 1, arq);
        if(!is_used){
            freeInode = (char)i;
            break;
        }
    }
    
    //insert inode
    int positionFirstInodeFree = inodeStart + freeInode * sizeof(INODE);
    fseek(arq, positionFirstInodeFree, SEEK_SET);
    INODE inode;
    inode.IS_DIR=0;
    inode.IS_USED=1;
    char name[strlen(filePath.c_str())];
    for(int i = 0; i < strlen(filePath.c_str()); i++){
        name[i] = 0; 
        if((i+1) < strlen(filePath.c_str())){
            name[i] = filePath.at(i+1);
        }
    } 
    static int flag, limitJ;
    bool IsInRoot = true;
    for(int i = 0; i < sizeof(name); i++){
        if(name[i] == 0x2F){ //if has /
            flag ++;
            IsInRoot = false; 
            limitJ = sizeof(name) - i + 1; 
        }
        if(flag){
            for(int j = 0; j < sizeof(name); j++){
                name[j] = 0; 
            } 
            for(int j = 0; j<sizeof(name) - limitJ; j++){
                i++;
                name[j] = filePath.at(i+1);
            }
            flag --; //if tree has size > 2
        }
    }
    std::copy(name, name + sizeof(name), inode.NAME); 
    int dif = 0;
    if(strlen(filePath.c_str()) < sizeof(inode.NAME)){
        dif = sizeof(inode.NAME) - strlen(filePath.c_str());
        for(int i = strlen(filePath.c_str()); i < strlen(filePath.c_str()) + dif; i++ ){
            inode.NAME[i] = 0; 
        }
    }
    inode.SIZE = fileContent.length(); 
    for (int i=0; i < 3; i++){
        inode.DIRECT_BLOCKS [i] = 0x00;
        inode.INDIRECT_BLOCKS[i] = 0x00;
        inode.DOUBLE_INDIRECT_BLOCKS[i] = 0x00;
    }
    fwrite(&inode, sizeof(INODE), 1, arq);
    
    //calculate blocks to store file
    int string_length = (int)fileContent.length(); 
    int number_blocks_to_use = 
    ceil((float)string_length/information_file_system[0]); 
    
    //find block free
    fseek(arq, 3, SEEK_SET);
    char map; //ok
    char firstFreeBlock;
    fread(&map, sizeof(char), 1, arq); //ok
    std::bitset<8> set8 = map;
    for(int i=0; i<set8.size(); i++){
        if(set8.test(i) == false && set8.test(i+1) == false){
            firstFreeBlock = i; //ok
            break;
        }
    }

    //block in inode
    for(int i = 0; i < number_blocks_to_use; i++){
        fseek(arq, positionFirstInodeFree + 13 + i, SEEK_SET);    
        fwrite(&firstFreeBlock, sizeof(char), 1, arq);
        firstFreeBlock ++; //ok
    }

    //update map with new block
    fseek(arq, 3, SEEK_SET);
    unsigned long newmap {0x00};
    fread(&newmap, sizeof(char), mapsize, arq); //ok
    std::bitset<16> updatemap = newmap; //ok
    std::size_t idx = 0;
    for (int i = 0; i < number_blocks_to_use; i++){
    while (idx < updatemap.size() && updatemap.test(idx)){
        ++idx;
    }
    updatemap.set(idx);
    }
    unsigned long newbitset = updatemap.to_ulong();
    fseek(arq, 3, SEEK_SET);
    fwrite(&newbitset, sizeof(char), 1, arq); //ok

    //update size of directory of file 
    char aux;
    static int dir;
    if(IsInRoot){
        dir = 0;
    }else{
        for(int i = 0; i < information_file_system[2]; i++){
            fseek(arq, inodeStart + 1 + sizeof(INODE) * i, SEEK_SET);
            char aux1;
            fread(&aux1, sizeof(char), 1, arq);
            dir ++;
            if (aux1 == 0){
                break;
            }
        }
    }
    fseek(arq, inodeStart + 12 + dir * sizeof(INODE), SEEK_SET);
    fread(&aux, sizeof(char), 1, arq);
    fseek(arq, -1, SEEK_CUR);
    aux++;
    fwrite(&aux, sizeof(char), 1, arq); //ok

    //assign block root if it doesnt exist
    int blockstart = inodeStart + information_file_system[2] * sizeof(INODE) + 1 ;
    fseek(arq, blockstart, SEEK_SET);
    char blockroot = 0x01;
    fwrite(&blockroot, sizeof(char), 1, arq);

    //assign block of dir
    if(dir){
        fseek(arq, blockstart + firstFreeBlock, SEEK_SET);
        fwrite(&freeInode, sizeof(char), 1, arq);
    }

    //assign block of dir/file if in root
    if(!dir){
        fseek(arq, blockstart + 1, SEEK_SET);
        char aux = freeInode;
        if (aux = 1){
            aux = aux -1;
        }
        fwrite(&aux, sizeof(char), 1, arq);
    }

    //assign block from file
    fseek(arq, positionFirstInodeFree + 13, SEEK_SET);
    char blocktostorefile;
    fread(&blocktostorefile, sizeof(char), 1, arq);
    fseek(arq, blockstart + blocktostorefile * information_file_system[0], SEEK_SET); //ok
    char contentfile[strlen(fileContent.c_str())];
    for (size_t i = 0; i < strlen(fileContent.c_str()); i++){
        contentfile[i] = fileContent.at(i);
    }
    fwrite(&contentfile, sizeof(char), strlen(fileContent.c_str()), arq);
    fclose(arq);
};

void addDir(std::string fsFileName, std::string dirPath){
    FILE *arq = fopen(fsFileName.c_str(), "r+");

    //get fs information 
    char information_file_system[3];
    fread(information_file_system, sizeof(char), 3, arq);

    //check free inode to store dir
    int freeInode;
    int mapsize = ceil((float)information_file_system[1]/8.0);  
    int inodeStart = 3 + mapsize;
    for (int i = 0; i < information_file_system[2]; i++){
        char is_used;
        fseek(arq, inodeStart + i * sizeof(INODE), SEEK_SET);
        fread(&is_used, sizeof(unsigned char), 1, arq);
        if(!is_used){
            freeInode = (char)i;
            break;
        }
    }
    
    //insert inode
    int positionFirstInodeFree = inodeStart + freeInode * sizeof(INODE);
    int pathlength = strlen(dirPath.c_str());
    fseek(arq, positionFirstInodeFree, SEEK_SET);
    INODE inode;
    inode.IS_DIR=1;
    inode.IS_USED=1;
    char name[pathlength];
    for(int i = 0; i < pathlength; i++){
        name[i] = 0;
        if((i+1) < pathlength){
            name[i] = dirPath.at(i+1);
        }
    } 
    static int count, limitJ;
    bool IsInRoot = true;
    for(int i = 0; i < pathlength; i++){
        if(name[i] == 0x2F){
            count ++;
            IsInRoot = false;
            limitJ = pathlength - i + 1;
        }
        if(count){ 
            for(int j = 0; j<pathlength - limitJ; j++){
                name[i - 1] = 0;
                name[i] = 0;
                name[i + 1] = 0;
                i++;
                name[j] = dirPath.at(i+1);
            }
            count --; //if tree has size > 2
        }
    }
    std::copy(name, name + pathlength, inode.NAME); 
    static int test;
    if(pathlength < sizeof(inode.NAME)){
        test = sizeof(inode.NAME) - pathlength;
        for(int i = pathlength; i < pathlength + test; i++ ){
            inode.NAME[i] = 0; 
        }
    }
    inode.SIZE = 0; 
    for (int i=0; i < 3; i++){
        inode.DIRECT_BLOCKS [i] = 0x00;
        inode.INDIRECT_BLOCKS[i] = 0x00;
        inode.DOUBLE_INDIRECT_BLOCKS[i] = 0x00;
    }
    fwrite(&inode, sizeof(INODE), 1, arq);
    
    //calculate blocks to store file
    int number_blocks_to_use = 1; 
    
    //find block free
    fseek(arq, 3, SEEK_SET);
    char map; 
    char firstFreeBlock;
    fread(&map, sizeof(char), 1, arq); 
    std::bitset<8> set8 = map;
    for(int i=0; i<set8.size(); i++){
        if(set8.test(i) == false){
            firstFreeBlock = i; 
            break;
        }
    }

    //block in inode
    for(int i = 0; i < number_blocks_to_use; i++){
        fseek(arq, positionFirstInodeFree + 13 + i, SEEK_SET);    
        fwrite(&firstFreeBlock, sizeof(char), 1, arq);
        firstFreeBlock ++; 
    }

    //update map with new block
    fseek(arq, 3, SEEK_SET);
    unsigned long newmap {0x00};
    fread(&newmap, sizeof(char), mapsize, arq); 
    std::bitset<16> updatemap = newmap; 
    std::size_t idx = 0;
    for (int i = 0; i < number_blocks_to_use; i++){
    while (idx < updatemap.size() && updatemap.test(idx)){
        ++idx;
    }
    updatemap.set(idx);
    }
    unsigned long newbitset = updatemap.to_ulong();
    fseek(arq, 3, SEEK_SET);
    fwrite(&newbitset, sizeof(char), 1, arq); 

    //update size of directory of dir
    char aux;
    static int dir;
    if(IsInRoot){
        dir = 0;
    }else{
        for(int i = 0; i < information_file_system[2]; i++){
            fseek(arq, inodeStart + 1 + sizeof(INODE) * i, SEEK_SET);
            char aux1;
            fread(&aux1, sizeof(char), 1, arq);
            dir ++;
            if (aux1 == 0){
                break;
            }
        }
    }
    fseek(arq, inodeStart + 12 + dir * sizeof(INODE), SEEK_SET);
    fread(&aux, sizeof(char), 1, arq);
    fseek(arq, inodeStart + 12 + dir * sizeof(INODE), SEEK_SET);
    aux++;
    fwrite(&aux, sizeof(char), 1, arq); //ok

    //assign block root if it doesnt exist
    int blockstart = inodeStart + information_file_system[2] * sizeof(INODE) + 1 ;
    fseek(arq, blockstart, SEEK_SET);
    char blockroot = 0x01;
    fwrite(&blockroot, sizeof(char), 1, arq);

    //assign block of dir of the file/dir
    if(dir){
        fseek(arq, blockstart + firstFreeBlock, SEEK_SET);
        fwrite(&freeInode, sizeof(char), 1, arq);
    }

    //assign block of dir/file if in root
    if(!dir){
        fseek(arq, blockstart + 1, SEEK_SET);
        char test = freeInode;
        fwrite(&test, sizeof(char), 1, arq);
    }
    fclose(arq);
};

void remove(std::string fsFileName, std::string path){
    FILE *arq = fopen(fsFileName.c_str(), "r+");

    //get fs information 
    char information_file_system[3];
    fread(information_file_system, sizeof(char), 3, arq);
    int mapsize = ceil((float)information_file_system[1]/8.0);  
    int inodeStart = 3 + mapsize;
    int blockstart = inodeStart + information_file_system[2] * sizeof(INODE) + 1 ;

    //read directory and file names
    char File[strlen(path.c_str())];
    for(int i = 0; i < sizeof(File); i++){
        File[i] = 0; //zera
        if((i+1) < strlen(path.c_str())){
            File[i] = path.at(i+1); //atribui valor da string
        }
    } 
    static int flag, limitJ;
    char Dir[strlen(path.c_str())];
    for(int j = 0; j < sizeof(Dir); j++){Dir[j] = 0;}
    for(int i = 0; i < sizeof(File); i++){
        if(File[i] == 0x2F){ //se tem /
            for(int j = 0; j < i; j++){
                Dir[j] = path.at(j + 1);
            }
            limitJ = sizeof(File) - i + 1; 
            flag ++;
            if(flag){
                for(int j = 0; j < sizeof(File); j++){File[j] = 0;} 
                for(int j = 0; j < sizeof(File) - limitJ; j++){
                    i++;
                    File[j] = path.at(i+1);
                }
                flag --;
            }
        }
    }

    //find directory and file index
    char auxDir[strlen(path.c_str())], auxFile[strlen(path.c_str())];
    for(int j = 0; j < strlen(path.c_str()); j++){auxDir[j] = 0;} 
    for(int j = 0; j < strlen(path.c_str()); j++){auxFile[j] = 0;} 
    int indexDirectoryFather, indexFileOrDir = 0;
    for(int i = 0; i < information_file_system[2]; i++){
        fseek(arq, inodeStart + 2 + i * sizeof(INODE), SEEK_SET);
        fread(&auxDir, sizeof(char), 10, arq);
        fseek(arq, -10, SEEK_CUR);
        fread(&auxFile, sizeof(char), 10, arq);
            if (!strcmp(auxDir, Dir)){
                indexDirectoryFather = i;
                if(indexDirectoryFather == information_file_system[2] - 1){
                    indexDirectoryFather = 0;
                }
            }
            if (!strcmp(auxFile, File)){
                indexFileOrDir = i;
            }
    }

    //remove
    static INODE inode;
    if(indexDirectoryFather && indexFileOrDir){
        fseek(arq, inodeStart + indexFileOrDir * sizeof(INODE), SEEK_SET);
        fwrite(&inode, sizeof(INODE), 1, arq);
        fseek(arq, inodeStart + indexDirectoryFather * sizeof(INODE) + 12, SEEK_SET);
        char zero = 0x00;
        fwrite(&zero, sizeof(char), 1, arq);
    }

    if(indexDirectoryFather && !indexFileOrDir){
        fseek(arq, inodeStart + indexDirectoryFather * sizeof(INODE), SEEK_SET);
        fwrite(&inode, sizeof(INODE), 1, arq);
    }

    if(!indexDirectoryFather && indexFileOrDir){
        if(indexFileOrDir == 1){
            fseek(arq, blockstart, SEEK_SET);
            char aux2 = 1;
            fread(&aux2, sizeof(char), 1, arq);
            aux2 ++;
            fseek(arq, -1, SEEK_CUR);
            fwrite(&aux2, sizeof(char), 1, arq);            
        }
        fseek(arq, blockstart + 1, SEEK_SET);
        char aux;
        fread(&aux, sizeof(char), 1, arq);
        aux --;
        fseek(arq, inodeStart + 12, SEEK_SET);
        fwrite(&aux, sizeof(char), 1, arq);
        //remove
        fseek(arq, inodeStart + indexFileOrDir * sizeof(INODE), SEEK_SET);
        fwrite(&inode, sizeof(INODE), 1, arq);
    }

    //update bitmap
    fseek(arq, 3, SEEK_SET);
    unsigned long newmap {0x00};
    fread(&newmap, sizeof(char), mapsize, arq); 
    std::bitset<16> updatemap = newmap; 
    std::size_t idx = 0;
    int aux = 2;
    if(!indexDirectoryFather && indexFileOrDir){
        aux = 2;
    }
    if(indexDirectoryFather && indexFileOrDir){
        aux = 3;
    }
    if(indexFileOrDir == 1){
        aux = 0;
        updatemap.set(1, 0);
    }
    for(int i = 0; i < aux; i++){
    while (idx < updatemap.size() && updatemap.test(idx)){
        ++idx;
    }
    updatemap.set(idx - i, 0);
    }
    unsigned long newbitset = updatemap.to_ulong();
    fseek(arq, 3, SEEK_SET);
    fwrite(&newbitset, sizeof(char), 1, arq); 

    fclose(arq);
};

void move(std::string fsFileName, std::string oldPath, std::string newPath){
    FILE *arq = fopen(fsFileName.c_str(), "r+");
    //get fs information 
    char information_file_system[3];
    fread(information_file_system, sizeof(char), 3, arq);
    int mapsize = ceil((float)information_file_system[1]/8.0);  
    int inodeStart = 3 + mapsize;
    int blockstart = inodeStart + information_file_system[2] * sizeof(INODE) + 1 ;

    //read oldpath
    char oldPathFile[strlen(oldPath.c_str())];
    for(int i = 0; i < sizeof(oldPathFile); i++){
        oldPathFile[i] = 0; 
        if((i+1) < strlen(oldPath.c_str())){
            oldPathFile[i] = oldPath.at(i+1); 
        }
    } 
    static int flag, limitJ;
    char OldPathDir[strlen(oldPath.c_str())];
    for(int j = 0; j < sizeof(OldPathDir); j++){OldPathDir[j] = 0;}
    for(int i = 0; i < sizeof(oldPathFile); i++){
        if(oldPathFile[i] == 0x2F){ 
            for(int j = 0; j < i; j++){
                OldPathDir[j] = oldPath.at(j + 1);
            }
            limitJ = sizeof(oldPathFile) - i + 1; 
            flag ++;
            if(flag){
                for(int j = 0; j < sizeof(oldPathFile); j++){oldPathFile[j] = 0;} 
                for(int j = 0; j < sizeof(oldPathFile) - limitJ; j++){
                    i++;
                    oldPathFile[j] = oldPath.at(i+1);
                }
                flag --;
            }
        }
    }

    //find directory and file of old path index
    char auxDirOldPath[strlen(oldPath.c_str())], auxFileOldPath[strlen(oldPath.c_str())];
    for(int j = 0; j < strlen(oldPath.c_str()); j++){auxDirOldPath[j] = 0;} 
    for(int j = 0; j < strlen(oldPath.c_str()); j++){auxFileOldPath[j] = 0;} 
    int indexDirOld, indexFileOld = 0;
    for(int i = 0; i < information_file_system[2]; i++){
        fseek(arq, inodeStart + 2 + i * sizeof(INODE), SEEK_SET);
        fread(&auxDirOldPath, sizeof(char), 10, arq);
        fseek(arq, -10, SEEK_CUR);
        fread(&auxFileOldPath, sizeof(char), 10, arq);
            if (!strcmp(auxDirOldPath, OldPathDir)){
                indexDirOld = i;
                if(indexDirOld == information_file_system[2] - 1){
                    indexDirOld = 0;
                }
            }
            if (!strcmp(auxFileOldPath, oldPathFile)){
                indexFileOld = i;
            }
    }

    //read newpath
    char newPathFile[strlen(newPath.c_str())];
    for(int i = 0; i < sizeof(newPathFile); i++){
        newPathFile[i] = 0; 
        if((i+1) < strlen(newPath.c_str())){
            newPathFile[i] = newPath.at(i+1); 
        }
    } 
    static int flagNew, limitJNew;
    char newPathDir[strlen(newPath.c_str())];
    for(int j = 0; j < sizeof(newPathDir); j++){newPathDir[j] = 0;}
    for(int i = 0; i < sizeof(newPathFile); i++){
        if(newPathFile[i] == 0x2F){
            for(int j = 0; j < i; j++){
                newPathDir[j] = newPath.at(j + 1);
            }
            limitJNew = sizeof(newPathFile) - i + 1; 
            flagNew ++;
            if(flagNew){
                for(int j = 0; j < sizeof(newPathFile); j++){newPathFile[j] = 0;} 
                for(int j = 0; j < sizeof(newPathFile) - limitJ; j++){
                    i++;
                    newPathFile[j] = newPath.at(i+1);
                }
                flagNew --;
            }
        }
    }

    //find directory and file of new path index
    char auxDirNewPath[strlen(newPath.c_str())], auxFileNewPath[strlen(newPath.c_str())];
    for(int j = 0; j < strlen(newPath.c_str()); j++){auxDirNewPath[j] = 0;} 
    for(int j = 0; j < strlen(newPath.c_str()); j++){auxFileNewPath[j] = 0;} 
    int indexDirNew, indexFileNew = 0;
    for(int i = 0; i < information_file_system[2]; i++){
        fseek(arq, inodeStart + 2 + i * sizeof(INODE), SEEK_SET);
        fread(&auxDirNewPath, sizeof(char), 10, arq);
        fseek(arq, -10, SEEK_CUR);
        fread(&auxFileNewPath, sizeof(char), 10, arq);
            if (!strcmp(auxDirNewPath, newPathDir)){
                indexDirNew = i;
                if(indexDirNew == information_file_system[2] - 1){
                    indexDirNew = 0;
                }
            }
            if (!strcmp(auxFileNewPath, newPathFile)){
                indexFileNew = i;
            }
    }

    if(indexDirOld && !indexDirNew){
        char aux = 0x00;
        fseek(arq, inodeStart + indexDirOld * sizeof(INODE) + 12 , SEEK_SET);
        fwrite(&aux, sizeof(char), 1, arq);
        aux = indexFileNew;
        fseek(arq, inodeStart + indexDirNew * sizeof(INODE) + 12, SEEK_SET);
        fwrite(&aux, sizeof(char), 1, arq);
        fseek(arq, 1, SEEK_CUR);
        char aux1 = aux * 2;
        fwrite(&aux1, sizeof(char), 1, arq);
        fseek(arq, 3, SEEK_SET);
        char map; 
        char firstFreeBlock;
        fread(&map, sizeof(char), 1, arq); 
        std::bitset<8> set8 = map;
        for(int i=0; i<set8.size(); i++){
            if(set8.test(i) == false && set8.test(i+1) == false){
                firstFreeBlock = i; 
                break;
            }
        }
        fseek(arq, blockstart + firstFreeBlock * 2, SEEK_SET);
        fwrite(&aux, sizeof(char), 1, arq);
        fseek(arq, 3, SEEK_SET);
        unsigned long newmap {0x00};
        fread(&newmap, sizeof(char), mapsize, arq); 
        std::bitset<16> updatemap = newmap; 
        std::size_t idx = 0;
        for (int i = 0; i < 1; i++){
        while (idx < updatemap.size() && updatemap.test(idx)){
            ++idx;
        }
        updatemap.set(idx);
        }
        unsigned long newbitset = updatemap.to_ulong();
        fseek(arq, 3, SEEK_SET);
        fwrite(&newbitset, sizeof(char), 1, arq); 
    }  
    if(!indexDirOld && indexDirNew){
        char aux = 0x02;
        fseek(arq, inodeStart + indexDirOld * sizeof(INODE) + 12 , SEEK_SET);
        fwrite(&aux, sizeof(char), 1, arq);
        fseek(arq, 1, SEEK_CUR);
        char aux1 = 0;
        fwrite(&aux1, sizeof(char), 1, arq);
        aux = indexFileNew;
        fseek(arq, inodeStart + indexDirNew * sizeof(INODE) + 12, SEEK_SET);
        fwrite(&aux, sizeof(char), 1, arq);
        fseek(arq, blockstart, SEEK_SET);
        char aux2 = 0x02;
        fwrite(&aux2, sizeof(char), 1, arq);
        aux2 = aux2 + 1;
        fwrite(&aux2, sizeof(char), 1, arq);
        fseek(arq, 3, SEEK_SET);
        char map; 
        char firstFreeBlock;
        fread(&map, sizeof(char), 1, arq); 
        std::bitset<8> set8 = map;
        for(int i=0; i<set8.size(); i++){
            if(set8.test(i) == false){
                firstFreeBlock = i;
                break;
            }
        }
        fseek(arq, blockstart + firstFreeBlock -1, SEEK_SET);
        char aux3 = 0x01;
        fwrite(&aux3, sizeof(char), 1, arq);
        fseek(arq, 3, SEEK_SET);
        unsigned long newmap {0x00};
        fread(&newmap, sizeof(char), mapsize, arq); 
        std::bitset<16> updatemap = newmap; 
        std::size_t idx = 0;
        while (idx < updatemap.size() && !updatemap.test(idx)){
            ++idx;
        }
        updatemap.set(idx + 6, 0);
        unsigned long newbitset = updatemap.to_ulong();
        fseek(arq, 3, SEEK_SET);
        fwrite(&newbitset, sizeof(char), 1, arq); 
    }
    if(!indexDirOld && !indexDirNew){
        fseek(arq, inodeStart + sizeof(INODE) * (indexFileOld) + 2, SEEK_SET);
        fwrite(&newPathFile, sizeof(newPathFile), 1, arq);
    }
    fclose(arq);
};
