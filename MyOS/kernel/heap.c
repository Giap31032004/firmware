#include "heap.h"
#include "kernel.h"

typedef struct A_BLOCK_LINK {
    struct A_BLOCK_LINK *pxNextFreeBlock; /* Chỉ dùng khi khối đang TRỐNG */
    size_t xBlockSize;                    /* Chứa kích thước + Bit đánh dấu đã cấp phát */
} BlockLink_t;

#define portBYTE_ALIGNMENT ( 8 )
#define portBYTE_ALIGNMENT_MASK ( 0x0007 )
#define heapBLOCK_ALLOCATED_BITMASK ( ( size_t ) 1 << 31 )

/* Kích thước của Header sau khi làm tròn (Đảm bảo luôn chia hết cho 8) */
static const size_t xHeapStructSize = ( sizeof( BlockLink_t ) + portBYTE_ALIGNMENT_MASK ) & ~portBYTE_ALIGNMENT_MASK;
/* Kích thước tối thiểu của một khối có thể bị cắt nhỏ */
#define heapMINIMUM_BLOCK_SIZE ( ( size_t ) ( xHeapStructSize * 2 ) )

static uint8_t ucHeap[ HEAP_SIZE ] __attribute__((aligned(8)));

static BlockLink_t xStart, *pxEnd = NULL;

/* Biến đo lường (Metrics) */
static size_t xFreeBytesRemaining = 0;
static size_t xMinimumEverFreeBytesRemaining = 0;
static void prvInsertBlockIntoFreeList( BlockLink_t *pxBlockToInsert ) {
    BlockLink_t *pxIterator;
    uint8_t *puc;

    /* 1. Tìm vị trí để chèn (Đảm bảo pxIterator < pxBlockToInsert < pxIterator->pxNextFreeBlock) */
    for( pxIterator = &xStart; pxIterator->pxNextFreeBlock < pxBlockToInsert; pxIterator = pxIterator->pxNextFreeBlock ) {
        /* Không làm gì cả, chỉ duyệt để tìm nút liền trước */
    }

    /* 2. Thử gộp với khối TRƯỚC nó (Nếu chúng nằm liền kề nhau trên RAM) */
    puc = ( uint8_t * ) pxIterator;
    if( ( puc + pxIterator->xBlockSize ) == ( uint8_t * ) pxBlockToInsert ) {
        pxIterator->xBlockSize += pxBlockToInsert->xBlockSize;
        pxBlockToInsert = pxIterator; // Nhập khối mới vào khối cũ
    }

    /* 3. Thử gộp với khối SAU nó (Nếu chúng nằm liền kề nhau trên RAM) */
    puc = ( uint8_t * ) pxBlockToInsert;
    if( ( puc + pxBlockToInsert->xBlockSize ) == ( uint8_t * ) pxIterator->pxNextFreeBlock ) {
        if( pxIterator->pxNextFreeBlock != pxEnd ) {
            /* Nối size và lấy pointer của thằng đằng sau */
            pxBlockToInsert->xBlockSize += pxIterator->pxNextFreeBlock->xBlockSize;
            pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock->pxNextFreeBlock;
        } else {
            pxBlockToInsert->pxNextFreeBlock = pxEnd;
        }
    } else {
        /* Không gộp được với thằng đằng sau, chỉ trỏ tới nó */
        pxBlockToInsert->pxNextFreeBlock = pxIterator->pxNextFreeBlock;
    }

    /* 4. Cập nhật lại liên kết nếu ở bước 2 KHÔNG gộp được với khối trước */
    if( pxIterator != pxBlockToInsert ) {
        pxIterator->pxNextFreeBlock = pxBlockToInsert;
    }
}

/* -------------------------------------------------------------------------
 * KHỞI TẠO HEAP
 * ------------------------------------------------------------------------- */
void os_mem_init( void ) {
    BlockLink_t *pxFirstFreeBlock;

    /* Nút ảo xStart trỏ tới đầu Heap */
    xStart.pxNextFreeBlock = ( void * ) ucHeap;
    xStart.xBlockSize = ( size_t ) 0;

    /* Nút ảo pxEnd nằm ở tít cuối Heap (đánh dấu kết thúc) */
    pxEnd = ( void * ) ( ucHeap + HEAP_SIZE - xHeapStructSize );
    pxEnd->xBlockSize = 0;
    pxEnd->pxNextFreeBlock = NULL;

    /* Khởi tạo khối trống đầu tiên ôm trọn toàn bộ RAM còn lại */
    pxFirstFreeBlock = ( void * ) ucHeap;
    pxFirstFreeBlock->xBlockSize = HEAP_SIZE - xHeapStructSize;
    pxFirstFreeBlock->pxNextFreeBlock = pxEnd;

    xFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
    xMinimumEverFreeBytesRemaining = pxFirstFreeBlock->xBlockSize;
}

/* -------------------------------------------------------------------------
 * CẤP PHÁT BỘ NHỚ (MALLOC)
 * ------------------------------------------------------------------------- */
void *os_malloc( size_t xWantedSize ) {
    BlockLink_t *pxBlock, *pxPreviousBlock, *pxNewBlockLink;
    void *pvReturn = NULL;

    /* Bỏ qua nếu xin 0 byte, và làm tròn kích thước lên bội số của 8 */
    if( xWantedSize > 0 ) {
        xWantedSize += xHeapStructSize; /* Cộng thêm kích thước của Header */
        /* Căn lề 8 bytes */
        if( ( xWantedSize & portBYTE_ALIGNMENT_MASK ) != 0 ) {
            xWantedSize += ( portBYTE_ALIGNMENT - ( xWantedSize & portBYTE_ALIGNMENT_MASK ) );
        }
    }

    OS_ENTER_CRITICAL(); /* AN TOÀN ĐA LUỒNG */
    {
        /* Kiểm tra hợp lệ và còn đủ RAM không */
        if( ( xWantedSize > 0 ) && ( xWantedSize <= xFreeBytesRemaining ) ) {
            /* Duyệt danh sách TRỐNG để tìm khối vừa vặn nhất (First-Fit) */
            pxPreviousBlock = &xStart;
            pxBlock = xStart.pxNextFreeBlock;

            while( ( pxBlock->xBlockSize < xWantedSize ) && ( pxBlock->pxNextFreeBlock != NULL ) ) {
                pxPreviousBlock = pxBlock;
                pxBlock = pxBlock->pxNextFreeBlock;
            }

            /* Nếu tìm thấy một khối hợp lệ (không phải là pxEnd) */
            if( pxBlock != pxEnd ) {
                /* Cắt khối này ra khỏi danh sách trống */
                pvReturn = ( void * ) ( ( ( uint8_t * ) pxPreviousBlock->pxNextFreeBlock ) + xHeapStructSize );
                pxPreviousBlock->pxNextFreeBlock = pxBlock->pxNextFreeBlock;

                /* Nếu khối tìm được quá to, CẮT NHỎ NÓ RA (Split) */
                if( ( pxBlock->xBlockSize - xWantedSize ) > heapMINIMUM_BLOCK_SIZE ) {
                    /* Tạo header mới cho phần còn dư */
                    pxNewBlockLink = ( void * ) ( ( ( uint8_t * ) pxBlock ) + xWantedSize );
                    pxNewBlockLink->xBlockSize = pxBlock->xBlockSize - xWantedSize;
                    pxBlock->xBlockSize = xWantedSize;

                    /* Ném phần dư này ngược lại vào danh sách trống */
                    prvInsertBlockIntoFreeList( pxNewBlockLink );
                }

                /* Cập nhật RAM còn trống */
                xFreeBytesRemaining -= pxBlock->xBlockSize;
                if( xFreeBytesRemaining < xMinimumEverFreeBytesRemaining ) {
                    xMinimumEverFreeBytesRemaining = xFreeBytesRemaining;
                }

                /* ĐÁNH DẤU LÀ ĐÃ CẤP PHÁT BẰNG BIT 31 (MSB) */
                pxBlock->xBlockSize |= heapBLOCK_ALLOCATED_BITMASK;
                pxBlock->pxNextFreeBlock = NULL;
            }
        }
    }
    OS_EXIT_CRITICAL();

    return pvReturn;
}

/* -------------------------------------------------------------------------
 * THU HỒI BỘ NHỚ (FREE)
 * ------------------------------------------------------------------------- */
void os_free( void *pv ) {
    uint8_t *puc = ( uint8_t * ) pv;
    BlockLink_t *pxLink;

    if( pv != NULL ) {
        /* Lùi con trỏ lại 8 bytes để lấy cái Header */
        puc -= xHeapStructSize;
        pxLink = ( void * ) puc;

        OS_ENTER_CRITICAL(); /* AN TOÀN ĐA LUỒNG */
        {
            /* Kiểm tra xem khối này có thực sự đang được cấp phát không? (Tránh free 2 lần) */
            if( ( pxLink->xBlockSize & heapBLOCK_ALLOCATED_BITMASK ) != 0 ) {
                if( pxLink->pxNextFreeBlock == NULL ) {
                    /* Xóa bit đánh dấu, trả về kích thước gốc */
                    pxLink->xBlockSize &= ~heapBLOCK_ALLOCATED_BITMASK;

                    /* Cập nhật lại RAM trống */
                    xFreeBytesRemaining += pxLink->xBlockSize;

                    /* Gọi thuật toán thần thánh để trả về mảng và TỰ ĐỘNG GỘP KHỐI */
                    prvInsertBlockIntoFreeList( ( BlockLink_t * ) pxLink );
                }
            }
        }
        OS_EXIT_CRITICAL();
    }
}

/* Hàm phụ trợ lấy thông tin cho giao diện Shell */
size_t os_get_free_heap_size( void ) {
    return xFreeBytesRemaining;
}

size_t os_get_minimum_ever_free_heap_size( void ) {
    return xMinimumEverFreeBytesRemaining;
}