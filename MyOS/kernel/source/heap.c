#include "heap.h"
#include "kernel.h"

typedef struct BLOCK_LINK {
    struct BLOCK_LINK *NextFreeBlock; /* Chỉ dùng khi khối đang TRỐNG */
    size_t BlockSize;                    /* Chứa kích thước + Bit đánh dấu đã cấp phát */
} BlockLink_t;

#define BYTE_ALIGNMENT ( 8 ) // căn lề bộ nhớ , 8 byte
#define BYTE_ALIGNMENT_MASK ( 0x0007 ) // mặt na bit để kiểm tra xem có chia hết cho 8 hay không
#define BLOCK_ALLOCATED_BITMASK ( ( size_t ) 1 << 31 ) // đánh dấu khối ram được cấp phát hay đang trống

/* Kích thước của Header sau khi làm tròn (Đảm bảo luôn chia hết cho 8) */
static const size_t HeapStructSize = ( sizeof( BlockLink_t ) + BYTE_ALIGNMENT_MASK ) & ~BYTE_ALIGNMENT_MASK;
/* Kích thước tối thiểu của một khối có thể bị cắt nhỏ */
#define MINIMUM_BLOCK_SIZE ( ( size_t ) ( HeapStructSize * 2 ) )

//static uint8_t ucHeap[ HEAP_SIZE ] __attribute__((aligned(8)));
/* LẤY CÁC CỘT MỐC TỪ FILE LINKER SCRIPT (linker.ld) */
extern uint32_t _end[];      /* Bắt đầu vùng RAM trống */
extern uint32_t _estack;   /* Đáy của Stack (cũng là đỉnh của RAM) */

static BlockLink_t Start, *End = NULL;

/* Biến đo lường (Metrics) */
static size_t FreeBytesRemaining = 0;
static size_t MinimumEverFreeBytesRemaining = 0;
static void InsertBlockIntoFreeList( BlockLink_t *BlockToInsert ) {
    BlockLink_t *Iterator;
    uint8_t *puc;

    /* 1. Tìm vị trí để chèn (Đảm bảo pxIterator < pxBlockToInsert < pxIterator->pxNextFreeBlock) */
    for( Iterator = &Start; Iterator->NextFreeBlock < BlockToInsert; Iterator = Iterator->NextFreeBlock ) {
        /* Không làm gì cả, chỉ duyệt để tìm nút liền trước */
    }

    /* 2. Thử gộp với khối TRƯỚC nó (Nếu chúng nằm liền kề nhau trên RAM) */
    puc = ( uint8_t * ) Iterator;
    if( ( puc + Iterator->BlockSize ) == ( uint8_t * ) BlockToInsert ) {
        Iterator->BlockSize += BlockToInsert->BlockSize;
        BlockToInsert = Iterator; // Nhập khối mới vào khối cũ
    }

    /* 3. Thử gộp với khối SAU nó (Nếu chúng nằm liền kề nhau trên RAM) */
    puc = ( uint8_t * ) BlockToInsert;
    if( ( puc + BlockToInsert->BlockSize ) == ( uint8_t * ) Iterator->NextFreeBlock ) {
        if( Iterator->NextFreeBlock != End ) {
            /* Nối size và lấy pointer của thằng đằng sau */
            BlockToInsert->BlockSize += Iterator->NextFreeBlock->BlockSize;
            BlockToInsert->NextFreeBlock = Iterator->NextFreeBlock->NextFreeBlock;
        } else {
            BlockToInsert->NextFreeBlock = End;
        }
    } else {
        /* Không gộp được với thằng đằng sau, chỉ trỏ tới nó */
        BlockToInsert->NextFreeBlock = Iterator->NextFreeBlock;
    }

    /* 4. Cập nhật lại liên kết nếu ở bước 2 KHÔNG gộp được với khối trước */
    if( Iterator != BlockToInsert ) {
        Iterator->NextFreeBlock = BlockToInsert;
    }
}

/* -------------------------------------------------------------------------
 * KHỞI TẠO HEAP (BẢN NÂNG CẤP CHO STM32F407)
 * ------------------------------------------------------------------------- */
void memory_init( void ) {
    BlockLink_t *FirstFreeBlock;
    
    /* 1. Lấy địa chỉ bắt đầu (Ép sang uint32_t trước để tính toán) */
    uint8_t *HeapStart = (uint8_t *)((uint32_t)&_end);

    /* Căn lề địa chỉ bắt đầu (Align 8 bytes) để CPU không bị lỗi HardFault khi truy cập */
    if( ( ( uint32_t ) HeapStart & BYTE_ALIGNMENT_MASK ) != 0 ) {
        HeapStart += ( BYTE_ALIGNMENT - ( ( uint32_t ) HeapStart & BYTE_ALIGNMENT_MASK ) );
    }

    /* 2. Lấy địa chỉ kết thúc (Ép sang uint32_t để lùi 4096 bytes mà GCC không báo lỗi) */
    uint8_t *HeapEnd = (uint8_t *)((uint32_t)&_estack - 4096);

    /* Tính toán Kích thước Heap linh hoạt (Lấy RAM tổng - RAM đã dùng) */
    size_t configTOTAL_HEAP_SIZE = (size_t)(HeapEnd - HeapStart);

    /* Nút ảo xStart trỏ tới đầu Heap */
    Start.NextFreeBlock = ( void * ) HeapStart;
    Start.BlockSize = ( size_t ) 0;

    /* Nút ảo pxEnd nằm ở tít cuối Heap (đánh dấu kết thúc) */
    End = ( void * ) ( HeapStart + configTOTAL_HEAP_SIZE - HeapStructSize );
    End->BlockSize = 0;
    End->NextFreeBlock = NULL;

    /* Khởi tạo khối trống đầu tiên ôm trọn toàn bộ RAM còn lại */
    FirstFreeBlock = ( void * ) HeapStart;
    FirstFreeBlock->BlockSize = configTOTAL_HEAP_SIZE - HeapStructSize;
    FirstFreeBlock->NextFreeBlock = End;

    FreeBytesRemaining = FirstFreeBlock->BlockSize;
MinimumEverFreeBytesRemaining = FirstFreeBlock->BlockSize;
}

/* -------------------------------------------------------------------------
 * CẤP PHÁT BỘ NHỚ (MALLOC)
 * ------------------------------------------------------------------------- */
void *os_malloc( size_t WantedSize ) {
    BlockLink_t *Block, *PreviousBlock, *NewBlockLink;
    void *Return = NULL;

    /* Bỏ qua nếu xin 0 byte, và làm tròn kích thước lên bội số của 8 */
    if( WantedSize > 0 ) {
        WantedSize += HeapStructSize; /* Cộng thêm kích thước của Header */
        /* Căn lề 8 bytes */
        if( ( WantedSize & BYTE_ALIGNMENT_MASK ) != 0 ) {
            WantedSize += ( BYTE_ALIGNMENT - ( WantedSize & BYTE_ALIGNMENT_MASK ) );
        }
    }

    OS_ENTER_CRITICAL(); /* AN TOÀN ĐA LUỒNG */
    {
        /* Kiểm tra hợp lệ và còn đủ RAM không */
        if( ( WantedSize > 0 ) && ( WantedSize <= FreeBytesRemaining ) ) {
            /* Duyệt danh sách TRỐNG để tìm khối vừa vặn nhất (First-Fit) */
            PreviousBlock = &Start;
            Block = Start.NextFreeBlock;

            while( ( Block->BlockSize < WantedSize ) && ( Block->NextFreeBlock != NULL ) ) {
                PreviousBlock = Block;
                Block = Block->NextFreeBlock;
            }

            /* Nếu tìm thấy một khối hợp lệ (không phải là pxEnd) */
            if( Block != End ) {
                /* Cắt khối này ra khỏi danh sách trống */
                Return = ( void * ) ( ( ( uint8_t * ) PreviousBlock->NextFreeBlock ) + HeapStructSize );
                PreviousBlock->NextFreeBlock = Block->NextFreeBlock;

                /* Nếu khối tìm được quá to, CẮT NHỎ NÓ RA (Split) */
                if( ( Block->BlockSize - WantedSize ) > MINIMUM_BLOCK_SIZE ) {
                    /* Tạo header mới cho phần còn dư */
                    NewBlockLink = ( void * ) ( ( ( uint8_t * ) Block ) + WantedSize );
                    NewBlockLink->BlockSize = Block->BlockSize - WantedSize;
                    Block->BlockSize = WantedSize;

                    /* Ném phần dư này ngược lại vào danh sách trống */
                    InsertBlockIntoFreeList( NewBlockLink );
                }

                /* Cập nhật RAM còn trống */
                FreeBytesRemaining -= Block->BlockSize;
                if( FreeBytesRemaining < MinimumEverFreeBytesRemaining ) {
                    MinimumEverFreeBytesRemaining = FreeBytesRemaining;
                }

                /* ĐÁNH DẤU LÀ ĐÃ CẤP PHÁT BẰNG BIT 31 (MSB) */
                Block->BlockSize |= BLOCK_ALLOCATED_BITMASK;
                Block->NextFreeBlock = NULL;
            }
        }
    }
    OS_EXIT_CRITICAL();

    return Return;
}

/* -------------------------------------------------------------------------
 * THU HỒI BỘ NHỚ (FREE)
 * ------------------------------------------------------------------------- */
void os_free( void *pv ) {
    uint8_t *puc = ( uint8_t * ) pv;
    BlockLink_t *Link;

    if( pv != NULL ) {
        /* Lùi con trỏ lại 8 bytes để lấy cái Header */
        puc -= HeapStructSize;
        Link = ( void * ) puc;

        OS_ENTER_CRITICAL(); /* AN TOÀN ĐA LUỒNG */
        {
            /* Kiểm tra xem khối này có thực sự đang được cấp phát không? (Tránh free 2 lần) */
            if( ( Link->BlockSize & BLOCK_ALLOCATED_BITMASK ) != 0 ) {
                if( Link->NextFreeBlock == NULL ) {
                    /* Xóa bit đánh dấu, trả về kích thước gốc */
                    Link->BlockSize &= ~BLOCK_ALLOCATED_BITMASK;

                    /* Cập nhật lại RAM trống */
                    FreeBytesRemaining += Link->BlockSize;

                    /* Gọi thuật toán thần thánh để trả về mảng và TỰ ĐỘNG GỘP KHỐI */
                    InsertBlockIntoFreeList( ( BlockLink_t * ) Link );
                }
            }
        }
        OS_EXIT_CRITICAL();
    }
}

/* Hàm phụ trợ lấy thông tin cho giao diện Shell */
size_t os_get_free_heap_size( void ) {
    return FreeBytesRemaining;
}

size_t os_get_minimum_ever_free_heap_size( void ) {
    return MinimumEverFreeBytesRemaining;
}