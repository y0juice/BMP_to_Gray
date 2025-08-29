#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

// 소벨 필터
int sobelX[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
};
int sobelY[3][3] = {
    {-1, -2, -1},
    { 0,  0,  0},
    { 1,  2,  1}
};
// 안전하게 픽셀 값 가져오기
unsigned char getPixel(unsigned char* image, int width, int height, int x, int y) {
    if (x < 0) x = 0;
    if (x >= width) x = width - 1;
    if (y < 0) y = 0;
    if (y >= height) y = height - 1;
    return image[y * width + x];
}
// 소벨 필터로 엣지 찾기
void findEdges(unsigned char* input, unsigned char* output, int width, int height) {
    printf("엣지 검출 중...\n");
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int gx = 0, gy = 0;
            // 주변 9개 픽셀에 필터 적용
            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    unsigned char pixel = getPixel(input, width, height, x + j, y + i);
                    gx += pixel * sobelX[i + 1][j + 1];
                    gy += pixel * sobelY[i + 1][j + 1];
                }
            }
            // 엣지 강도 계산
            int edge = (int)sqrt(gx * gx + gy * gy);
            if (edge > 255) edge = 255;
            output[y * width + x] = edge;
        }
    }
}


// BMP 파일 헤더 구조체
#pragma pack(push, 1)     // pragma = 데이터를 붙혀주는 역할
typedef struct {
    uint16_t type;        // 파일 형식 (BM)
    uint32_t size;        // 파일 크기
    uint16_t reserved1;   // 예약된 필드
    uint16_t reserved2;   // 예약된 필드
    uint32_t offset;      // 이미지 데이터 시작 위치
} BMPFileHeader;

typedef struct {
    uint32_t size;        // 헤더 크기
    int32_t width;        // 이미지 너비
    int32_t height;       // 이미지 높이
    uint16_t planes;      // 색상 평면 수
    uint16_t bitCount;    // 픽셀당 비트 수
    uint32_t compression; // 압축 형식
    uint32_t sizeImage;   // 이미지 데이터 크기
    int32_t xPelsPerMeter;// 수평 해상도
    int32_t yPelsPerMeter;// 수직 해상도
    uint32_t clrUsed;     // 사용된 색상 수
    uint32_t clrImportant;// 중요한 색상 수
} BMPInfoHeader;

typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} RGB;
#pragma pack(pop)

// RGB를 그레이스케일로 변환하는 함수
uint8_t rgbToGrayscale(RGB pixel) {
    // 표준 luminance 공식: Y = 0.299*R + 0.587*G + 0.114*B
    return (uint8_t)(0.299 * pixel.red + 0.587 * pixel.green + 0.114 * pixel.blue);
}

int main() {
    const char* inputFile = "a.bmp";
    const char* outputFile = "output_a.bmp";
    const int IMAGE_WIDTH = 1247;
    const int IMAGE_HEIGHT = 720;
    const char* edgeFile = "output_a_edge.bmp";        // 엣지 검출 BMP 파일 추가
    const char* memFile = "output_a.mem";  // MEM 파일 이름 추가
   
    FILE* edgeOutFile = NULL;                        // 엣지 검출 BMP 파일 포인터 추가
    FILE* memOutFile = NULL;  // MEM 파일 포인터 추가
    FILE* inFile = NULL;
    FILE* outFile = NULL;

    // 입력 파일 열기
    if (fopen_s(&inFile, inputFile, "rb") != 0 || inFile == NULL) {
        printf("입력 파일을 열 수 없습니다: %s\n", inputFile);
        return 1;
    }

    // BMP 헤더 읽기
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;

    fread(&fileHeader, sizeof(BMPFileHeader), 1, inFile);
    fread(&infoHeader, sizeof(BMPInfoHeader), 1, inFile);

    // BMP 파일 형식 검증
    if (fileHeader.type != 0x4D42) { // "BM"
        printf("유효하지 않은 BMP 파일입니다.\n");
        fclose(inFile);
        return 1;
    }

    if (infoHeader.bitCount != 24) {
        printf("24비트 BMP 파일이 아닙니다. (현재: %d비트)\n", infoHeader.bitCount);
        fclose(inFile);
        return 1;
    }

    if (infoHeader.width != IMAGE_WIDTH || infoHeader.height != IMAGE_HEIGHT) {
        printf("이미지 크기가 %dx%d가 아닙니다. (현재: %dx%d)\n",
            IMAGE_WIDTH, IMAGE_HEIGHT, infoHeader.width, infoHeader.height);
        fclose(inFile);
        return 1;
    }

    printf("입력 파일 정보:\n");
    printf("- 크기: %dx%d\n", infoHeader.width, infoHeader.height);
    printf("- 비트 수: %d\n", infoHeader.bitCount);
    printf("- 파일 크기: %d bytes\n", fileHeader.size);

    // 패딩 계산 (BMP는 각 행이 4바이트 경계에 정렬되어야 함)
    int padding = (4 - ((IMAGE_WIDTH * 3) % 4)) % 4;
    int rowSize = IMAGE_WIDTH * 3 + padding;

    // 이미지 데이터 메모리 할당
    RGB* imageData = (RGB*)malloc(IMAGE_WIDTH * IMAGE_HEIGHT * sizeof(RGB));
    if (imageData == NULL) {
        printf("메모리 할당 실패\n");
        fclose(inFile);
        return 1;
    }

    // 이미지 데이터 읽기 (BMP는 아래쪽부터 저장됨)
    fseek(inFile, fileHeader.offset, SEEK_SET);

    for (int y = IMAGE_HEIGHT - 1; y >= 0; y--) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            fread(&imageData[y * IMAGE_WIDTH + x], sizeof(RGB), 1, inFile);
        }
        // 패딩 건너뛰기
        fseek(inFile, padding, SEEK_CUR);
    }

    fclose(inFile);

    // 그레이스케일 변환
    uint8_t* grayscaleData = (uint8_t*)malloc(IMAGE_WIDTH * IMAGE_HEIGHT);
    if (grayscaleData == NULL) {
        printf("메모리 할당 실패\n");
        free(imageData);
        return 1;
    }

    for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
        grayscaleData[i] = rgbToGrayscale(imageData[i]);
    }

    // 엣지 검출을 위한 메모리 할당 및 처리 (새로 추가)
    uint8_t* edgeData = (uint8_t*)malloc(IMAGE_WIDTH * IMAGE_HEIGHT);
    if (edgeData == NULL) {
        printf("엣지 데이터 메모리 할당 실패\n");
        free(imageData);
        free(grayscaleData);
        return 1;
    }

    // 소벨 필터로 엣지 검출
    findEdges(grayscaleData, edgeData, IMAGE_WIDTH, IMAGE_HEIGHT);


    // 출력 파일 생성
    if (fopen_s(&outFile, outputFile, "wb") != 0 || outFile == NULL) {
        printf("출력 파일을 생성할 수 없습니다: %s\n", outputFile);
        free(imageData);
        free(grayscaleData);
        return 1;
    }

    // 4. 엣지 검출 BMP 파일 저장 (기존 BMP 저장 후 추가)
// 엣지 검출 결과를 BMP로 저장
    if (fopen_s(&edgeOutFile, edgeFile, "wb") != 0 || edgeOutFile == NULL) {
        printf("엣지 출력 파일을 생성할 수 없습니다: %s\n", edgeFile);
        free(imageData);
        free(grayscaleData);
        free(edgeData);
        return 1;
    }


    // 8비트 그레이스케일 BMP 헤더 설정
    int newPadding = (4 - (IMAGE_WIDTH % 4)) % 4;
    int newRowSize = IMAGE_WIDTH + newPadding;
    int paletteSize = 256 * 4; // 256색 팔레트 (각 항목은 4바이트)

    BMPFileHeader newFileHeader = fileHeader;
    BMPInfoHeader newInfoHeader = infoHeader;

    newFileHeader.size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + paletteSize + (newRowSize * IMAGE_HEIGHT);
    newFileHeader.offset = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + paletteSize;

    newInfoHeader.bitCount = 8;
    newInfoHeader.sizeImage = newRowSize * IMAGE_HEIGHT;
    newInfoHeader.clrUsed = 256;
    newInfoHeader.clrImportant = 256;

    // 헤더 쓰기
    fwrite(&newFileHeader, sizeof(BMPFileHeader), 1, outFile);
    fwrite(&newInfoHeader, sizeof(BMPInfoHeader), 1, outFile);

    // 엣지 검출 BMP 헤더는 그레이스케일과 동일
    fwrite(&newFileHeader, sizeof(BMPFileHeader), 1, edgeOutFile);
    fwrite(&newInfoHeader, sizeof(BMPInfoHeader), 1, edgeOutFile);

    // 그레이스케일 팔레트 생성 및 쓰기
    for (int i = 0; i < 256; i++) {
        uint8_t color[4] = { i, i, i, 0 }; // Blue, Green, Red, Reserved
        fwrite(color, 4, 1, outFile);
        fwrite(color, 4, 1, edgeOutFile);
    }

    // 그레이스케일 이미지 데이터 쓰기 (아래쪽부터)
    uint8_t paddingBytes[4] = { 0 };

    for (int y = IMAGE_HEIGHT - 1; y >= 0; y--) {
        for (int x = 0; x < IMAGE_WIDTH; x++) {
            fwrite(&grayscaleData[y * IMAGE_WIDTH + x], 1, 1, outFile);
        }
        // 패딩 쓰기
        if (newPadding > 0) {
            fwrite(paddingBytes, newPadding, 1, outFile);
        }
    }
    for (int y = IMAGE_HEIGHT - 1; y >= 0; y--) {
    for (int x = 0; x < IMAGE_WIDTH; x++) {
        fwrite(&edgeData[y * IMAGE_WIDTH + x], 1, 1, edgeOutFile);
    }
    if (newPadding > 0) {
        fwrite(paddingBytes, newPadding, 1, edgeOutFile);
    }
}
fclose(edgeOutFile);

// 엣지 데이터 쓰기
for (int y = IMAGE_HEIGHT - 1; y >= 0; y--) {
    for (int x = 0; x < IMAGE_WIDTH; x++) {
        fwrite(&edgeData[y * IMAGE_WIDTH + x], 1, 1, edgeOutFile);
    }
    if (newPadding > 0) {
        fwrite(paddingBytes, newPadding, 1, edgeOutFile);
    }
}
fclose(edgeOutFile);

    // 2. ======== MEM 파일 생성 부분 (추가) ========

    // MEM 파일 생성
    if (fopen_s(&memOutFile, memFile, "w") != 0 || memOutFile == NULL) {
        printf("MEM 파일을 생성할 수 없습니다: %s\n", memFile);
        free(imageData);
        free(grayscaleData);
        return 1;
    }

    // MEM 파일에 그레이스케일 데이터를 16진수로 쓰기
    // 각 픽셀값을 2자리 16진수로 변환하고 줄바꿈(0x0D 0x0A)으로 구분
    for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT; i++) {
        fprintf(memOutFile, "%02X\r\n", grayscaleData[i]);
    }

    fclose(memOutFile);

   
    fclose(outFile);

    // 메모리 해제
    free(imageData);
    free(grayscaleData);
    free(edgeData);                                  // 엣지 데이터 메모리 해제 추가

    printf("\n변환 완료!\n");
    printf("출력 파일: %s\n", outputFile);
    printf("그레이스케일 8비트 BMP로 저장되었습니다.\n");
    // 3. ================================================
    printf("Verilog MEM 파일: %s\n", memFile);  // MEM 파일 생성 완료 메시지 추가
    printf("엣지 검출 BMP 파일: %s\n", edgeFile);        // 엣지 BMP 파일 메시지 추가



    return 0;
}