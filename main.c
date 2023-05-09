
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/*======================================================================*/

/* BEGIN USER Define */
#define BYTE_BIT        6
#define SET_BIT         14
#define TAG_BIT         12
#define BYTE_MASK       0x0000003F //0000_0000_0000_0000 0000_0000_0011_1111
#define SET_MASK        0x000FFFC0 //0000_0000_0000_1111 1111_1111_1100_0000
#define MESI_BIT        2          //4 states ~ 2 bits
#define DATA_WAYS       4          //4-ways associtive cache
#define DATA_LRU        DATA_WAYS  //4-ways => max 2 bits
#define INSTR_WAYS      2          //2-ways associtive cache
#define INSTR_LRU       INSTR_WAYS //2-ways => max 1 bit
#define MAX_TRACES      500        //Number of characters allowed in a trace line
#define NUM_OF_SET      16384      //Total number of set entries
/* BEGIN USER Define */

/*======================================================================*/

/* BEGIN USER Typedef */
/* Hold the value for each operation type */
typedef enum {
    READ            = 0,
    WRITE           = 1,
    FETCH           = 2,
    EVICT           = 3,
    REQ_L2          = 4,
    RESET_AND_CLEAR = 8,
    PRINT_LOG       = 9
} Operation_Typedef;

/* Every L1 cache type: LRU/MESI/tag/set/byte 
*
*     LRU   MESI      tag          set       byte    Data
*   --------------------------------------------------------
*   | 1/2 |  2  |     12     |      14     |  *6* | [Data] |
*   --------------------------------------------------------
*/
typedef struct {
    uint16_t tag;
    uint16_t set;
    uint8_t LRU_State; //LRU state: 00 = LR_State, 11 = MR_State
    uint8_t Valid;
    uint8_t Dirty;
    uint32_t address;
} Cache_Line_Typedef;

/* Report information: hit times, miss time, read/write access times, hit ratio */
//L1 Data Cache
typedef struct {
    uint32_t Data_Hit;
    uint32_t Data_Miss;
    uint32_t Data_Read_Access;
    uint32_t Data_Write_Access;
    float Data_Hit_Ratio;
} Data_Cache_Stats_Typedef;

//L1 Instr Cache
typedef struct {
    uint32_t Instruction_Hit;
    uint32_t Instruction_Miss;
    uint32_t Instruction_Read_Access;
    uint32_t Instruction_Write_Access;
    float Instr_Hit_Ratio;
} Instr_Cache_Stats_Typedef;
/* END USER Typedef */

/*======================================================================*/

/* BEGIN USER Variable */
//Data and Instructino cache declarations
Cache_Line_Typedef Data_Cache[DATA_WAYS][16384];
Cache_Line_Typedef Instr_Cache[INSTR_WAYS][16384];

//Report information declaration
Data_Cache_Stats_Typedef  Data_Stats_Report;
Instr_Cache_Stats_Typedef Instr_Stats_Report;

//Debug Mode
unsigned int Mode = 3;
/* END USER Variable */

/*======================================================================*/

/* BEGIN USER PFP */
bool Reset_And_Clear_Cache();
unsigned int Selection_Menu();
FILE* Open_Trace_File(char* Trace_File);
bool Read_and_Run_Trace_File(FILE* fd);
//Cache operations
bool Data_Cache_Read(unsigned int address);
bool Data_Cache_Write(unsigned int address);
bool Instruction_Cache_Fetch(unsigned int address);
bool L2_Evict_Command_to_L1(unsigned int address);
bool Print_Content_And_State();
//Support functions
int Data_Match_Find(unsigned int Input_Tag, unsigned int Input_Set);
int Instruction_Match_Find(unsigned int Input_Tag, unsigned int Input_Set);
void Data_LRU_State_Update(unsigned int Set_Way, unsigned int Cache_Set, uint8_t Empty_Flag);
void Instruction_LRU_State_Update(unsigned int Set_Way, unsigned int Cache_Set, uint8_t Empty_Flag);
int Data_LRU_Smallest_Find(uint16_t Set_Index);
int Instruction_LRU_Smallest_Find(uint16_t Set_Index);
/* END USER PFP */

/*======================================================================*/

/* BEGIN MAIN PROGRAM */
int main(int argc, char* argv[])
{    
    /* BEGIN Main: Local variable */
    char *trace_file_name;    
    FILE *fd = NULL;
    /* END Main: Local variable */
    
    /* BEGIN Code */
    //Check input traces
    if (argc != 2)
    {
        printf("\033[1;31mERROR: Trace file name not found!\033[1;0m\n");
        exit(1);
    }
    else
    {
        trace_file_name = argv[1];
        printf("\033[1;32mTrace file name: %s\n\033[1;0m", trace_file_name);
    }
    
    //Clear cache and stats
    if (Reset_And_Clear_Cache()) {}
    else
    {
        printf("\033[1;31mERROR: Cannot reset and clear cache!\033[1;0m\n");
        exit(1);
    }

    //Select report mode
    Mode = (unsigned int) Selection_Menu();
    printf("\033[1;32mMode: %d\033[1;0m\n", Mode);
    //Read trace file    
    if ((fd = Open_Trace_File(trace_file_name))) 
    { 
        printf("\033[1;32mTrace file is opened successfully!\033[1;0m\n");
    }
    else
    {
        printf("\033[1;31mERROR: Cannot open trace file!\033[1;0m\n");
    }
    
    //Read and Run the Simulation
    if (Read_and_Run_Trace_File(fd) == true) {}
    else
    {
        printf("\033[1;31mERROR: Cannot read and simulate trace file!\033[1;0m\n");
    }
    

    //FINISH MESSAGE
    printf("\033[1;32mTEST FINISHED!\033[1;0m\n");
    /* END Code */

    return 0;
}
/* END MAIN PROGRAM */

/*======================================================================*/

/* BEGIN User function */
bool Reset_And_Clear_Cache()
{
    bool OK = false;
    printf("\033[1;32mResetting all cache lines and stats...\033[1;0m\n");

    uint16_t Tag_Mask = pow(2, TAG_BIT);
    uint16_t Nums_of_Sets = pow(2, SET_BIT);

    //Clearing Data Cache Lines
    for (uint8_t i = 0; i < DATA_WAYS; i++)
    {                
        for(unsigned int j = 0; j < Nums_of_Sets; j++)
        {                    
            Data_Cache[i][j].tag = Tag_Mask;
            Data_Cache[i][j].set = 0;
            Data_Cache[i][j].LRU_State = 0;
            Data_Cache[i][j].Valid = 0;
            Data_Cache[i][j].Dirty = 0;
            Data_Cache[i][j].address = 0;
        }                   
    }
    
    //Clear Instruction Cache Lines
    for (uint8_t i = 0; i < INSTR_WAYS; i++)
    {        
        for (unsigned int j = 0; j < Nums_of_Sets; j++)
        {
            Instr_Cache[i][j].tag = Tag_Mask;
            Instr_Cache[i][j].set = 0;
            Instr_Cache[i][j].LRU_State = 0;
            Instr_Cache[i][j].Valid = 0;
            Instr_Cache[i][j].Dirty = 0;
            Instr_Cache[i][j].address = 0;
        }        
    }
    
    //Clear Data Stats Information
    Data_Stats_Report.Data_Hit = 0;
    Data_Stats_Report.Data_Miss = 0;
    Data_Stats_Report.Data_Read_Access = 0;
    Data_Stats_Report.Data_Write_Access = 0;
    Data_Stats_Report.Data_Hit_Ratio = 0.0;
    //Clear Instruction Stats Information
    Instr_Stats_Report.Instruction_Hit = 0;
    Instr_Stats_Report.Instruction_Miss = 0;
    Instr_Stats_Report.Instruction_Read_Access = 0;
    Instr_Stats_Report.Instruction_Write_Access = 0;
    Instr_Stats_Report.Instr_Hit_Ratio = 0.0;

    //Done message
    printf("\033[1;32mDone resetting and clearing caches/stats\033[1;0m\n");
    return OK = true;    
}

unsigned int Selection_Menu()
{       
    unsigned int mode = 3;

    printf("\n\t\033[1;32mMode: \n");    
    printf("Mode 0: Usage statistics (hits, misses, read/write accesses) and command\n");
    printf("Mode 1: Information in Mode 0 along with communication message between L1 and L2\n");
    printf("Mode 2: Information in Mode 0 and 1 and operation information\n");

    do {
        printf("PLEASE SELECT A MODE: ");
        scanf("%u", &mode);
        printf("THE ENTERED MODE IS: %u\033[1;0m\n", mode);
    } while (mode > 2);    

    return mode;
}

FILE* Open_Trace_File(char* Trace_File)
{        
    FILE *fd = fopen(Trace_File, "r");    
    if (fd == NULL)
    {        
        return NULL;
    }
    else return fd;
}

bool Read_and_Run_Trace_File(FILE* fd)
{
    bool OK = false;

    char one_trace_line[MAX_TRACES];
    char* comments = NULL;
    unsigned int tmp_operation;    
    uint32_t address;

    while (fgets(one_trace_line, MAX_TRACES, fd) != NULL)
    {
        //Use "sscanf" to extract the value of ops and address
        if (Mode > 1) printf("\033[1;35mThe read text line: %s\033[1;0m\n", one_trace_line);
        sscanf(one_trace_line, "%u %x %c", &tmp_operation, &address, comments);
        if (Mode > 1) printf("\033[1;35mOperation: %u and address %u\033[1;0m\n", tmp_operation, address);
        if (one_trace_line[0]=='#'||!strcmp(one_trace_line, "\n")||!strcmp(one_trace_line, " ")||!strcmp(one_trace_line, "/")||!strcmp(one_trace_line, "*")||!strcmp(one_trace_line, "=")||(tmp_operation > 9)) 
        {
            __asm__("nop"); //Prevent unwanted/comment line from called into the cache simulation
        }
        else
        {            
            switch (tmp_operation)
            {
            case READ:
                if (Mode > 1) printf("\n\033[1;33m*-------READ from address 0x%x\033[1;0m\n", address);
                if (true == Data_Cache_Read(address))
                {       
                    if (Mode > 1)
                    {
                        printf("\033[1;31mERROR: L1 Data Cache read failed at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }
                else
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;33mRead Data successfully at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }                                
                break;
            
            case WRITE:
                if (Mode > 1) printf("\n\033[1;33m*-------WRITE from address 0x%x\033[1;0m\n", address);
                if (true == Data_Cache_Write(address))
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;31mERROR: L1 Data Cache write failed at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }
                else
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;33mWrite Data successfully at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }
                break;
            
            case FETCH:
                if (Mode > 1) printf("\033[1;33m\n*-------FETCH from address 0x%x\033[1;0m\n", address);
                if (true == Instruction_Cache_Fetch(address))
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;31mERROR: L1 Instruction Cache write failed at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }
                else
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;33mRead Instruction successfully at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }                                
                break;

            case EVICT:
                if (Mode > 1) printf("\n\033[1;33m*-------EVICT address command from L2 at address 0x%x\033[1;0m\n", address);
                if (true == L2_Evict_Command_to_L1(address))
                {
                    if (Mode > 1) 
                    {   
                        printf("\033[1;31mERROR: Evict line failed at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }
                else
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;33mEvict line successfully at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }
                                
                break;                

            case RESET_AND_CLEAR:
                if (Mode > 1) printf("\n\033[1;33m*-------RESET AND CLEAR\033[1;0m\n");
                if (false == Reset_And_Clear_Cache())
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;31mERROR: Reset and Clear failed at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }
                else
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;33mReset and Clear successfully at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }                                                
                break;

            case PRINT_LOG:
                if (Mode > 1) printf("\n\033[1;33m*-------PRINT LOG\033[1;0m\n");
                if (true == Print_Content_And_State())
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;31mERROR: Print Log failed at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                }
                else
                {
                    if (Mode > 1) 
                    {
                        printf("\033[1;33mPrint Log successfully at address 0x%x\n", address);
                        printf("=============================================================================\033[1;0m\n");
                    } 
                } 
                break;

            default:
                if (Mode > 1)
                {
                    printf("\033[1;31mERROR: Ivalid operation!\033[1;0m\n");
                }
                
                break;
            }
        }
    }
    return OK = true;
}

//Cache operations
bool Data_Cache_Read(unsigned int address)
{
    uint16_t Tag = address >> (SET_BIT + BYTE_BIT);
    uint16_t Set = (address & SET_MASK) >> BYTE_BIT;
    int Selected_Cache_Way = -1;
    uint8_t Empty_Flag = 0;

    //Update read access to stats
    Data_Stats_Report.Data_Read_Access++;

    //Find match in a set: -1 = miss; 0/1/2/3 = hit in a way of the corresponding set
    Selected_Cache_Way = Data_Match_Find(Tag, Set);        
    //Read Simulation when hit occur
    if (Selected_Cache_Way > -1)
    {
        //If contain VALID data
        if (Data_Cache[Selected_Cache_Way][Set].Valid)
        {
            Data_Stats_Report.Data_Hit++;
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = Data_Cache[Selected_Cache_Way][Set].Valid;
            Data_Cache[Selected_Cache_Way][Set].Dirty = Data_Cache[Selected_Cache_Way][Set].Dirty;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            //Additional debug message
            if (Mode > 1)
            {
                printf("\033[1;33m Data Cache READ HIT!\033[1;0m\n");
            }
        }
        //If no VALID data is in the cache line - This is nearly impossible since it has tag and no valid => not reasonable
        else
        {
            //Additional debug message
            if (Mode > 1)
            {
                printf("\033[1;33mData Cache READ MISS!\033[1;0m\n");
                printf("\033[1;33mReason: SAME TAG BUT INVALID! - CAN CAUSED BY EVICTION FROM L2!\033[1;0m\n");             
            }
            if (Mode > 0)
            {
                printf("\033[1;36mMessage to L2: Read from L2 block contain address 0x%x\033[1;0m\n",address);
            }
            Data_Stats_Report.Data_Miss++;
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = 1;
            Data_Cache[Selected_Cache_Way][Set].Dirty = Data_Cache[Selected_Cache_Way][Set].Dirty;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
        }                
    }
    else //READ MISS - MISS unoccupied/MISS replacement
    {        
        Data_Stats_Report.Data_Miss++;
        uint16_t Tag_Mask = pow(2, TAG_BIT);
        //Find empty cache line (empty way in set)
        for (uint8_t i = 0; (Selected_Cache_Way<0) && (i < DATA_WAYS); i++)
        {            
            if (Data_Cache[i][Set].tag == Tag_Mask)
            {
                Selected_Cache_Way = i;
                Empty_Flag = 1;
            }            
        }
        //Load the data into the empty line - MISS unoccupied
        if (Selected_Cache_Way > -1)
        {
            //Communication message: Req to L2
            if (Mode >= 2)
            {
                printf("\033[1;33mData Cache READ MISS!\033[1;0m\n");
                printf("\033[1;33mReason: Load block into empty line!\033[1;0m\n");
            }
            if (Mode >= 1)
            {
                printf("\033[1;36mMessage to L2: Read from L2 block contain address: 0x%x\033[1;0m\n", address);
            }   
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = 1;
            Data_Cache[Selected_Cache_Way][Set].Dirty = Data_Cache[Selected_Cache_Way][Set].Dirty;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);         
        }  
        else //MISS conflict
        {
            if (Mode > 1)
            {
                printf("\033[1;33mData Cache READ MISS!\033[1;0m\n");                
            }        
            //If no empty line (way) in the set => evict either INVALID line or least recently line
            for (uint8_t i = 0; i < DATA_WAYS; i++) //Find INVALID line - Mostly error as invalid = empty line
            {
                if (Data_Cache[i][Set].Valid == 0) 
                {
                    Selected_Cache_Way = i;
                }
            }
            //No INVALID line => evict smallest LRU line
            if (Selected_Cache_Way < 0) 
            {
                Selected_Cache_Way = Data_LRU_Smallest_Find(Set);
                /* 2 Case:
                Case 1: Line is clean => evict and read from L2
                Case 2: Line is not clean => write to L2 and read from L2
                */                
                if (Selected_Cache_Way > -1)
                {
                    if (Mode > 1)
                    {
                        printf("\033[1;33mReason: The line is occupied and conflict!\033[1;0m\n");
                    }
                    if (0 == Data_Cache[Selected_Cache_Way][Set].Dirty)
                    {
                        if (Mode > 1)
                        {
                            printf("\033[1;33mL1 evict line with address: 0x%x\n\033[1;0m", Data_Cache[Selected_Cache_Way][Set].address);
                        }                        
                        if (Mode > 0)
                        {                            
                            printf("\033[1;36mMessage to L2: Read line with address 0x%x from L2\033[1;0m\n", address);              
                        }
                        Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                        Data_Cache[Selected_Cache_Way][Set].set = Set;
                        Data_Cache[Selected_Cache_Way][Set].Valid = 1;
                        Data_Cache[Selected_Cache_Way][Set].Dirty = 0;
                        Data_Cache[Selected_Cache_Way][Set].address = address;
                        Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                    }
                    else
                    {                        
                        if (Mode > 0)
                        {
                            printf("\033[1;36mMessage to L2: Write back line with address 0x%x to L2\033[1;0m\n", Data_Cache[Selected_Cache_Way][Set].address);
                            printf("\033[1;36mMessage to L2: Read line with address 0x%x from L2\033[1;0m\n", address);                    
                        }
                        Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                        Data_Cache[Selected_Cache_Way][Set].set = Set;
                        Data_Cache[Selected_Cache_Way][Set].Valid = 1;
                        Data_Cache[Selected_Cache_Way][Set].Dirty = 0;
                        Data_Cache[Selected_Cache_Way][Set].address = address;
                        Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                    }                    
                }
                else
                {                
                    printf("\033[1;31mERROR: READ - THE LRU DATA IS CORRUPTED!\033[1;0m\n");
                    return true;
                }                
            }
            else //Load follow INVALID line
            {
                if (Mode > 1)
                {
                    printf("\033[1;33mReason:Can be caused by eviction command from L2!\033[1;0m\n");
                    printf("\033[1;33mL1 Data Cache Evict line with address 0x%x\033[1;0m\n", Data_Cache[Selected_Cache_Way][Set].address);
                }
                if (Mode > 0)
                {
                    printf("\033[1;36mMessage to L2: Read line with address 0x%x from L2\033[1;0m\n", address);
                }
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].Valid = 1;
                Data_Cache[Selected_Cache_Way][Set].Dirty = Data_Cache[Selected_Cache_Way][Set].Dirty;
                Data_Cache[Selected_Cache_Way][Set].address = address;
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            }                        
        }              
    }        
    return false;
}

bool Data_Cache_Write(unsigned int address)
{
    uint16_t Tag = address >> (SET_BIT + BYTE_BIT);
    uint16_t Set = (address & SET_MASK) >> BYTE_BIT;
    int Selected_Cache_Way = -1;
    uint8_t Empty_Flag = 0;

    //Update write access to stats
    Data_Stats_Report.Data_Write_Access++;

    //Find match in a set: -1 = miss; 0/1/2/3 = hit in a way of the corresponding set
    Selected_Cache_Way = Data_Match_Find(Tag, Set);        
    //Write Simulation when hit occur
    if (Selected_Cache_Way > -1)
    {
        if (Data_Cache[Selected_Cache_Way][Set].Valid == 1)
        {
            Data_Stats_Report.Data_Hit++;
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = Data_Cache[Selected_Cache_Way][Set].Valid;
            Data_Cache[Selected_Cache_Way][Set].Dirty = 1;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            if (Mode > 1)
            {
                printf("\033[1;33mData Cache WRITE HIT!\033[1;0m\n");
            }
        }
        else
        {
            //Additional debug message
            if (Mode > 1)
            {
                printf("\033[1;33mData Cache WRITE MISS!\n");
                printf("Reason: SAME TAG BUT INVALID! - THIS IS ACTUALLY ERROR!\033[1;0m\n");                
            }
            if (Mode > 0)
            {
                printf("\033[1;36mMessage to L2: Read for Ownership from L2 block contain address 0x%x\033[1;0m\n",address);
            }
            Data_Stats_Report.Data_Miss++;
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = 1;
            Data_Cache[Selected_Cache_Way][Set].Dirty = 1;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
        }                   
    }
    else
    {
        Data_Stats_Report.Data_Miss++;
        uint16_t Tag_Mask = pow(2, TAG_BIT);
        //Additional debug message

        //Find empty cache line (empty way in set)
        for (uint8_t i = 0; (Selected_Cache_Way<0) && (i < DATA_WAYS); i++)
        {            
            if (Data_Cache[i][Set].tag == Tag_Mask)
            {
                Selected_Cache_Way = i;
                Empty_Flag = 1;
            }            
        }
        //Load the data into the empty line - MISS unoccupied
        if (Selected_Cache_Way > -1)
        {
            if (Mode > 1)
            {
                printf("\033[1;33mData Cache WRITE MISS!\n");
                printf("Reason: Load block into empty line!\033[1;0m\n");
            }
            if (Mode > 0)
            {
                printf("\033[1;36mMessage to L2: Read for Ownership from L2 block contain address: 0x%x\033[1;0m\n", address);          
            }
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = 1;
            Data_Cache[Selected_Cache_Way][Set].Dirty = 1;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);   
        }
        else //MISS conflict
        {
            if (Mode > 1)
            {
                printf("\033[1;33mData Cache WRITE MISS!\n");
                printf("Reason:Invalid line (caused by other core/master) or Another line is occupied the LRU and to be written position!\033[1;0m\n");
            }
            //If no empty line (way) in the set => evict either INVALID line or least recently line
            for (uint8_t i = 0; i < DATA_WAYS; i++) //Find INVALID line - Mostly error as invalid = empty line
            {
                if (Data_Cache[i][Set].Valid == 0) 
                {
                    Selected_Cache_Way = i;
                }
            }
            //No INVALID line => evict smallest LRU line
            if (Selected_Cache_Way < 0) 
            {
                Selected_Cache_Way = Data_LRU_Smallest_Find(Set);
                /* 2 Case:
                Case 1: Line is clean => evict and read from L2
                Case 2: Line is not clean => write to L2 and read from L2
                */
                if (Selected_Cache_Way > -1)
                {
                    if (0 == Data_Cache[Selected_Cache_Way][Set].Dirty)
                    {
                        if (Mode > 1)
                        {
                            printf("\033[1;33mL1 evict line with address: 0x%x\033[1;0m\n", Data_Cache[Selected_Cache_Way][Set].address);
                        }                    
                        if (Mode > 0)
                        {                        
                            printf("\033[1;36mMessage to L2: Read for Ownership line with address 0x%x from L2\033[1;0m\n", address);        
                        }
                        Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                        Data_Cache[Selected_Cache_Way][Set].set = Set;
                        Data_Cache[Selected_Cache_Way][Set].Valid = 1;
                        Data_Cache[Selected_Cache_Way][Set].Dirty = 1;
                        Data_Cache[Selected_Cache_Way][Set].address = address;
                        Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                    }
                    else
                    {
                        if (Mode > 0)
                        {
                            printf("\033[1;36mMessage to L2: Write back line with address 0x%x to L2\n", Data_Cache[Selected_Cache_Way][Set].address);
                            printf("Message to L2: Read for Ownership line with address 0x%x from L2\033[1;0m\n", address);             
                        }
                        Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                        Data_Cache[Selected_Cache_Way][Set].set = Set;
                        Data_Cache[Selected_Cache_Way][Set].Valid = 1;
                        Data_Cache[Selected_Cache_Way][Set].Dirty = 1;
                        Data_Cache[Selected_Cache_Way][Set].address = address;
                        Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                    } 
                }               
                else
                {
                    printf("\033[1;31mERROR: WRITE - THE LRU DATA IS CORRUPTED!\033[1;0m\n");
                    return true;
                }                                                
            }
            //INVALID line => due to other core (not in project scope)
            else
            {
                if (Mode > 1)
                {
                    printf("\033[1;33mL1 Data Cache Evict line with address 0x%x\033[1;0m\n", Data_Cache[Selected_Cache_Way][Set].address);
                }
                if (Mode > 0)
                {
                    printf("\033[1;36mMessage to L2: Read for Ownership line with address 0x%x from L2\033[1;0m\n", address);
                }
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].Valid = 1;
                Data_Cache[Selected_Cache_Way][Set].Dirty = 1;
                Data_Cache[Selected_Cache_Way][Set].address = address;
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            }
            
        }
        
    }
    return false;
}

bool Instruction_Cache_Fetch(unsigned int address)
{
    uint16_t Tag = address >> (SET_BIT + BYTE_BIT);
    uint16_t Set = (address & SET_MASK) >> BYTE_BIT;
    int Selected_Cache_Way = -1;
    uint8_t Empty_Flag = 0;

    //Update read access to stats
    Instr_Stats_Report.Instruction_Read_Access++;

    //Find match in a set: -1 = miss; 0/1/2/3 = hit in a way of the corresponding set
    Selected_Cache_Way = Instruction_Match_Find(Tag, Set);
    //Read Simulation when hit occur
    if (Selected_Cache_Way > -1)
    {
        //If contain VALID instruction
        if (Instr_Cache[Selected_Cache_Way][Set].Valid)
        {
            Instr_Stats_Report.Instruction_Hit++;
            Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
            Instr_Cache[Selected_Cache_Way][Set].set = Set;
            Instr_Cache[Selected_Cache_Way][Set].Valid = Instr_Cache[Selected_Cache_Way][Set].Valid;
            Instr_Cache[Selected_Cache_Way][Set].Dirty = Instr_Cache[Selected_Cache_Way][Set].Dirty;
            Instr_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            //Additional debug message
            if (Mode >= 2)
            {
                printf("Instruction Cache READ HIT!\n");
            }
        }
        //If no VALID instruction is in the cache line - Due to eviction command from L2
        else
        {
            //Additional debug message
            if (Mode > 1)
            {
                printf("Instruction Cache READ MISS!\n");
                printf("Reason: L2 evict the line!\n");                
            }
            if (Mode > 0)
            {
                printf("Message to L2: Read from L2 block contain address 0x%x \n",address);
            }
            Instr_Stats_Report.Instruction_Miss++;
            Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
            Instr_Cache[Selected_Cache_Way][Set].set = Set;
            Instr_Cache[Selected_Cache_Way][Set].Valid = 1;
            Instr_Cache[Selected_Cache_Way][Set].Dirty = Instr_Cache[Selected_Cache_Way][Set].Dirty;
            Instr_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
        }
    }
    else
    {
        Instr_Stats_Report.Instruction_Miss++;
        uint16_t Tag_Mask = pow(2, TAG_BIT);
        //Find empty cache line (empty way in set)
        for (uint8_t i = 0; (Selected_Cache_Way<0) && (i < INSTR_WAYS); i++)
        {            
            if (Instr_Cache[i][Set].tag == Tag_Mask)
            {
                Selected_Cache_Way = i;
                Empty_Flag = 1;
            }            
        }
        //Load the instruction into the empty line - MISS unoccupied
        if (Selected_Cache_Way > -1)
        {
            //Communication message: Req to L2
            if (Mode >= 2)
            {
                printf("Instruction Cache READ MISS!\n");
                printf("Reason: Load block into empty line!\n");
            }
            if (Mode >= 1)
            {
                printf("Message to L2: Read from L2 block contain address: 0x%x\n", address);
            }   
            Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
            Instr_Cache[Selected_Cache_Way][Set].set = Set;
            Instr_Cache[Selected_Cache_Way][Set].Valid = 1;
            Instr_Cache[Selected_Cache_Way][Set].Dirty = Instr_Cache[Selected_Cache_Way][Set].Dirty;
            Instr_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);         
        }
        else //MISS conflict
        {
            if (Mode > 1)
            {
                printf("Instruction Cache READ MISS!\n");
                printf("Reason:Invalid line (cause by other core/master) or Another line is occupied the LRU and to be written position!\n");
            }        
            //If no empty line (way) in the set => evict either INVALID line or least recently line
            for (uint8_t i = 0; i < INSTR_WAYS; i++) //Find INVALID line - Mostly error as invalid = empty line
            {
                if (Instr_Cache[i][Set].Valid == 0) 
                {
                    Selected_Cache_Way = i;
                }
            }
            //No INVALID line => evict smallest LRU line
            if (Selected_Cache_Way < 0)
            {
                Selected_Cache_Way = Instruction_LRU_Smallest_Find(Set);
                /* 2 Case:
                Case 1: Line is clean => evict and read from L2
                Case 2: Line is not clean => write to L2 and read from L2
                */
                if (Selected_Cache_Way > -1)
                {
                    if (0 == Instr_Cache[Selected_Cache_Way][Set].Dirty)
                    {
                        if (Mode > 0)
                        {
                            printf("Message to L2: Evict the line at address: 0x%x\n", Data_Cache[Selected_Cache_Way][Set].address);
                            printf("Message to L2: Read line with address 0x%x from L2\n", address);                    
                        }
                        Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
                        Instr_Cache[Selected_Cache_Way][Set].set = Set;
                        Instr_Cache[Selected_Cache_Way][Set].Valid = 1;
                        Instr_Cache[Selected_Cache_Way][Set].Dirty = 0;
                        Instr_Cache[Selected_Cache_Way][Set].address = address;
                        Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                    }
                    else //This case never happen because nothing write to instruction cache
                    {
                        if (Mode > 1)
                        {
                            printf("ERROR: Dirty is set to 1 in instruction cache!\n");
                        }
                    }
                }
                else
                {
                    printf("ERROR: READ - THE LRU INSTRUCTION IS CORRUPTED!\n");
                    return true;
                }                
            }
            else //Load follow INVALID line
            {
                if (Mode > 1)
                {
                    printf("L1 Instruction Cache Evict line with address 0x%x\n", Data_Cache[Selected_Cache_Way][Set].address);
                }
                if (Mode > 0)
                {
                    printf("Message to L2: Read line with address 0x%x from L2\n", address);
                }
                Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
                Instr_Cache[Selected_Cache_Way][Set].set = Set;
                Instr_Cache[Selected_Cache_Way][Set].Valid = 1;
                Instr_Cache[Selected_Cache_Way][Set].Dirty = Instr_Cache[Selected_Cache_Way][Set].Dirty;
                Instr_Cache[Selected_Cache_Way][Set].address = address;
                Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            }
            
        }
        
    }
    return false;
}

bool L2_Evict_Command_to_L1(unsigned int address)
{
    /* L2 evict a line will cause L1 to evict a line
    In order to keep the inclusive (coherences) between L1 and L2
    The corresponding line in L1 must also be evicted
    This can be done simply by INVALIDATING the coressponded line.
    -------------------------------------------------------------------
    By doing this, when a read/write occurs
    => The line must be loaded again from L2 (causing L2 to loaded from memory)
    Hence, the second case in the "Read/Write Hit - INVALID case"*/
    uint16_t Tag = address >> (SET_BIT + BYTE_BIT);
    uint16_t Set = (address & SET_MASK) >> BYTE_BIT;
    
    //Search the Tag of a line in a set and invalidate the line once the tag is hit
    for (uint8_t i = 0; i < DATA_WAYS; i++)
    {
        if (Mode > 1) printf("Input address Tag: %u\n", Tag);
        if (Mode > 1) printf("Search Tag: %u\n", Data_Cache[i][Set].tag);
        if (Mode > 1) printf("Iteration: %u\n", i);
        if (Data_Cache[i][Set].tag == Tag)
        {
            if (Data_Cache[i][Set].Valid == 1)
            {
                if (Data_Cache[i][Set].Dirty == 0)
                {
                    if (Mode > 1)
                    {
                        printf("The line in L1 is NOT DIRTY => Can be evict without write back to L2!\n");                            
                    }
                    Data_Cache[i][Set].tag = Tag;
                    Data_Cache[i][Set].set = Set;
                    Data_Cache[i][Set].Valid = 0;
                    //Data_Cache[i][Set].Dirty = Data_Cache[i][Set].Dirty;
                    Data_Cache[i][Set].address = address;
                }
                else
                {
                    if (Mode > 1)
                    {
                        printf("The line in L1 is DIRTY => Have to write back to L2!\n");                            
                    }
                    if (Mode > 0)
                    {
                        printf("Message to L2: Write line with address 0x%x to L2\n", address);
                    }
                    Data_Cache[i][Set].tag = Tag;
                    Data_Cache[i][Set].set = Set;
                    Data_Cache[i][Set].Valid = 0;
                    //Data_Cache[i][Set].Dirty = Data_Cache[i][Set].Dirty;
                    Data_Cache[i][Set].address = address;                    
                }                                
            }
            else
            {
                if (Mode > 1)
                {
                    printf("The line is already invalid => L2 can evict safely\n");
                }
                Data_Cache[i][Set].tag = Tag;
                Data_Cache[i][Set].set = Set;                                
                Data_Cache[i][Set].address = address;
            }                        
        }
        else
        {
            if ((Mode > 1) && (i >= 3))
            {
                printf("ERROR: LINE NOT FOUND IN L1!\n");
            }
            if (i >= 3) return true;            
        }
                
    }
    return false;    
}

bool Print_Content_And_State()
{


    
    return false;
}

//Support functions
int Data_Match_Find(unsigned int Input_Tag, unsigned int Input_Set)
{        
    for (uint8_t i = 0; i < DATA_WAYS; i++)
    {
        if (Data_Cache[i][Input_Set].tag == Input_Tag)
        {
            return i;
        }        
    }
    return -1;
}

int Instruction_Match_Find(unsigned int Input_Tag, unsigned int Input_Set)
{
    for (uint8_t i = 0; i < INSTR_WAYS; i++)
    {
        if (Instr_Cache[i][Input_Set].tag == Input_Tag)
        {
            return i;
        }        
    }
    return -1;
}

void Data_LRU_State_Update(unsigned int Set_Way, unsigned int Cache_Set, uint8_t Empty_Flag)
{
    uint8_t LRU_Current_State = Data_Cache[Set_Way][Cache_Set].LRU_State;

    if (0 == Empty_Flag)    //Usuallay, in case a miss and no empty line in set
    {        
        for (uint8_t i = 0; i < DATA_LRU; i++)
        {
            if (LRU_Current_State > Data_Cache[i][Cache_Set].LRU_State) 
            {
                __asm__("nop");
            }
            else
            {
                --Data_Cache[i][Cache_Set].LRU_State;
            }            
        }        
    }
    else                    //Usually, in case of a miss and there is empty line in set
    {
        for (uint8_t i = 0; i < Set_Way; i++)
        {
            if (Data_Cache[i][Cache_Set].LRU_State < 4)
            {
                --Data_Cache[i][Cache_Set].LRU_State;
            }
            else
            {
                if (Mode > 1) printf("\033[1;31mERROR: LRU DATA CORRUPTED\033[1;0m\n");
            }                         
        }        
    }
    Data_Cache[Set_Way][Cache_Set].LRU_State = 3; //Highest state since it have just been referred to
}

void Instruction_LRU_State_Update(unsigned int Set_Way, unsigned int Cache_Set, uint8_t Empty_Flag)
{
    uint8_t LRU_Current_State = Instr_Cache[Set_Way][Cache_Set].LRU_State;

    if (0 == Empty_Flag)    //Usuallay, in case a hit occurred => decrease all other line by 1
    {        
        for (uint8_t i = 0; i < INSTR_LRU; i++)
        {
            if (LRU_Current_State > Instr_Cache[i][Cache_Set].LRU_State) 
            {
                __asm__("nop");
            }
            else
            {
                --Instr_Cache[i][Cache_Set].LRU_State;
            }            
        }        
    }
    else                    //Usually, in case of a miss
    {
        for (uint8_t i = 0; i < Set_Way; i++)
        {
            --Instr_Cache[i][Cache_Set].LRU_State;
        }        
    }
    Instr_Cache[Set_Way][Cache_Set].LRU_State = 1; //Highest state since it have just been referred to
}

int Data_LRU_Smallest_Find(uint16_t Set_Index)
{
    for (uint8_t i = 0; i < DATA_WAYS; i++)
    {
        if (Data_Cache[i][Set_Index].LRU_State == 0)
        {
            return i;
        }        
    }
    return -1;
}

int Instruction_LRU_Smallest_Find(uint16_t Set_Index)
{
    for (uint8_t i = 0; i < INSTR_WAYS; i++)
    {
        if (Instr_Cache[i][Set_Index].LRU_State == 0)
        {
            return i;
        }        
    }
    return -1;
}
/* END User function */