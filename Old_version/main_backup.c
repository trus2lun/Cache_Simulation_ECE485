
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
#define MAX_TRACES      100        //Support upto 1024 traces
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
    char MESI;
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
//Support functions
int Data_Match_Find(unsigned int Input_Tag, unsigned int Input_Set);
void Data_LRU_State_Update(unsigned int Set_Way, unsigned int Cache_Set, uint8_t Empty_Flag);
int Data_LRU_Smallest_Find(uint16_t Set_Index);
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
        printf("ERROR: Trace file name not found!\n\n");
        exit(1);
    }
    else
    {
        trace_file_name = argv[1];
        printf("Trace file name: %s\n", trace_file_name);
    }
    
    //Clear cache and stats
    if (Reset_And_Clear_Cache()) {}
    else
    {
        printf("ERROR: Cannot reset and clear cache!\n");
        exit(1);
    }

    //Select report mode
    Mode = (unsigned int) Selection_Menu();
    printf("Mode: %d\n", Mode);
    //Read trace file    
    if ((fd = Open_Trace_File(trace_file_name))) 
    { 
        __asm__("nop");
    }
    else
    {
        printf("ERROR: Cannot open trace file!\n");
    }
    
    //Read and Run the Simulation
    if (Read_and_Run_Trace_File(fd) == true) {}
    else
    {
        printf("ERROR: Cannot read and simulate trace file!\n");
    }
    

    //FINISH MESSAGE
    printf("TEST FINISHED!\n");
    /* END Code */

    return 0;
}
/* END MAIN PROGRAM */

/*======================================================================*/

/* BEGIN User function */
bool Reset_And_Clear_Cache()
{
    bool OK = false;
    printf("Resetting all cache lines and stats...\n");

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
            Data_Cache[i][j].MESI = 'I';
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
            Instr_Cache[i][j].MESI = 'I';
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
    printf("Done resetting and clearing caches/stats\n");
    return OK = true;    
}

unsigned int Selection_Menu()
{       
    unsigned int mode = 3;

    printf("\n\t Mode: \n");    
    printf("Mode 0: Usage statistics (hits, misses, read/write accesses) and command\n");
    printf("Mode 1: Information in Mode 0 along with communication message between L1 and L2\n");
    printf("Mode 2: Information in Mode 0 and 1 and operation information\n");

    do {
        printf("PLEASE SELECT A MODE: ");
        scanf("%u", &mode);
    } while (mode > 2);
    
    if (mode <=2)
    {
        printf("THE ENTERED MODE IS: %u\n", mode);
    }    

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
    unsigned int tmp_operation;    
    uint32_t address;

    while (fgets(one_trace_line, MAX_TRACES, fd) != NULL)
    {
        //Use "sscanf" to extract the value of ops and address
        sscanf(one_trace_line, "%u %x", &tmp_operation, &address);
        if (one_trace_line[0]=='#'||!strcmp(one_trace_line, "\n")||!strcmp(one_trace_line, " ")) 
        {
            __asm__("nop"); //Prevent unwanted line from called into the cache simulation
        }
        else
        {            
            switch (tmp_operation)
            {
            case READ:
                if (Mode >= 2) printf("*-------READ from address %x\n", address);
                if (true == Data_Cache_Read(address))
                {       
                    printf("ERROR: L1 Cache read failed at address %x\n", address);
                }
                else
                {
                    if (Mode >= 2) printf("Read successfully at address %x\n", address);
                }
                                
                break;
            
            case WRITE:
                if (Mode >= 2) printf("WRITE from address %x\n", address);
                if (true == Data_Cache_Write(address))
                {
                    printf("ERROR: L1 Cache write failed at address %x\n", address);
                }
                else
                {
                    if (Mode >= 2) printf("Write successfully at address %x\n", address);
                }
                break;
            
            case FETCH:
                if (Mode >= 2) printf("FETCH from address %x\n", address);
                break;

            case EVICT:
                if (Mode >= 2) printf("EVICT from address %x\n", address);
                break;

            //Có thể thêm case ở đây tùy vào các mode    

            case RESET_AND_CLEAR:
                if (Mode >= 2) printf("RESET AND CLEAR\n");
                break;

            case PRINT_LOG:
                if (Mode >= 2) printf("PRINT LOG\n");
                break;

            default:
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
        switch (Data_Cache[Selected_Cache_Way][Set].MESI)
        {
            case 'M':
                Data_Stats_Report.Data_Hit++;
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'M';
                Data_Cache[Selected_Cache_Way][Set].address = address;
                //update LRU
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                //Additional debug message
                if (Mode >= 2)
                {
                    printf("Data Cache READ HIT!\n");
                }
            break;

            case 'E':
                Data_Stats_Report.Data_Hit++;
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'E';
                Data_Cache[Selected_Cache_Way][Set].address = address;
                //update LRU
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                //Additional debug message
                if (Mode >= 2)
                {
                    printf("Data Cache READ HIT!\n");
                }
            break;

            case 'S':
                Data_Stats_Report.Data_Hit++;
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'S';
                Data_Cache[Selected_Cache_Way][Set].address = address;
                //update LRU
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                //Additional debug message
                if (Mode >= 2)
                {
                    printf("Data Cache READ HIT!\n");
                }
            break;

            case 'I': //This state of MESI is special => cause a miss
                Data_Stats_Report.Data_Miss++;
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'S';
                Data_Cache[Selected_Cache_Way][Set].address = address;
                //update LRU
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                //Additional debug message
                if (Mode >= 2)
                {
                    printf("Data Cache READ MISS: Read from L2 block contain address 0x%x\n",address);
                }
            break;

            default:
            break;
        }
    }
    else
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
        //Load the data into the empty line
        if (Selected_Cache_Way > -1)
        {
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].MESI = 'S';
            Data_Cache[Selected_Cache_Way][Set].address = address;
            //update LRU
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            //Communication message: Req to L2
            if (Mode > 0)
            {
                printf("Data Cache READ MISS: Request from L2 at address: 0x%x\n", address);
            }            
        }  
        else
        {
            if (Mode > 0)
            {
                printf("Data Cache READ MISS: Request from L2 at address: 0x%x\n", address);
            }            
            //If no empty line (way) in the set => evict either INVALID line or least recently line
            for (uint8_t i = 0; i < DATA_WAYS; i++) //Find INVALID line
            {
                if (Data_Cache[i][Set].MESI == 'I') 
                {
                    Selected_Cache_Way = i;
                }
            }
            if (Selected_Cache_Way < 0) //No INVALID line => evict smallest LRU line
            {
                Selected_Cache_Way = Data_LRU_Smallest_Find(Set);
                if (Mode >= 2)
                {
                    printf("READ: Replace Line %u in Set %u!\n", Selected_Cache_Way, Set);
                }
                if (Selected_Cache_Way > -1)
                {
                    Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                    Data_Cache[Selected_Cache_Way][Set].set = Set;
                    Data_Cache[Selected_Cache_Way][Set].MESI = 'E';
                    Data_Cache[Selected_Cache_Way][Set].address = address;
                    Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                }
                else
                {
                    printf("ERROR: READ - THE LRU DATA IS CORRUPTED!\n");
                    return true;
                }                
            }
            else //Load follow INVALID line
            {
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'E';
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

    //Update read access to stats
    Data_Stats_Report.Data_Write_Access++;

    //Find match in a set: -1 = miss; 0/1/2/3 = hit in a way of the corresponding set
    Selected_Cache_Way = Data_Match_Find(Tag, Set);        
    //Read Simulation when hit occur
    if (Selected_Cache_Way > -1)
    {
        switch (Data_Cache[Selected_Cache_Way][Set].MESI)
        {
            case 'M':
                Data_Stats_Report.Data_Hit++;
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'M';
                Data_Cache[Selected_Cache_Way][Set].address = address;
                //update LRU
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                //Additional debug message
                if (Mode >= 2)
                {
                    printf("Data Cache WRITE HIT!\n");
                }
            break;

            case 'E':
                Data_Stats_Report.Data_Hit++;
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'M';
                Data_Cache[Selected_Cache_Way][Set].address = address;
                //update LRU
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                //Additional debug message
                if (Mode >= 2)
                {
                    printf("Data Cache WRITE HIT!\n");
                }
            break;

            case 'S':
                Data_Stats_Report.Data_Hit++;
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'M';
                Data_Cache[Selected_Cache_Way][Set].address = address;
                //update LRU
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                //Additional debug message
                if (Mode >= 2)
                {
                    printf("Data Cache WRITE HIT!\n");
                }
            break;

            case 'I': //This state of MESI is special => cause a miss
                Data_Stats_Report.Data_Miss++;
                Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                Data_Cache[Selected_Cache_Way][Set].set = Set;
                Data_Cache[Selected_Cache_Way][Set].MESI = 'M';
                Data_Cache[Selected_Cache_Way][Set].address = address;
                //update LRU
                Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                //Additional debug message
                if (Mode >= 2)
                {
                    printf("Data Cache MISS: Read for Ownership from L2 block contain address 0x%x\n",address);
                }
            break;

            default:
            break;
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
    }
    
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

void Data_LRU_State_Update(unsigned int Set_Way, unsigned int Cache_Set, uint8_t Empty_Flag)
{
    uint8_t LRU_Current_State = Data_Cache[Set_Way][Cache_Set].LRU_State;

    if (0 == Empty_Flag)    //Usuallay, in case a hit occurred => decrease all other line by 1
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
    else                    //Usually, in case of a miss
    {
        for (uint8_t i = 0; i < Set_Way; i++)
        {
            --Data_Cache[i][Cache_Set].LRU_State;
        }        
    }
    Data_Cache[Set_Way][Cache_Set].LRU_State = 3; //Highest state since it have just been referred to
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
/* END User function */