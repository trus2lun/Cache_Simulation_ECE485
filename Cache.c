
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

/* Every L1 cache type: LRU/D/V/tag/set/byte 
*
*     LRU   D   V       tag          set       byte    Data
*   ----------------------------------------------------------
*   | 1/2 | 1 | 1 |     12     |      14     |  *6* | [Data] |
*   ----------------------------------------------------------
*/
typedef struct {
    uint16_t tag;
    uint16_t set;
    uint8_t LRU_State; //LRU state: 4 for DATA Cache, 2 for INSTRUCTION Cache
    uint8_t Valid;
    uint8_t Dirty;
    uint32_t address;
} Cache_Line_Typedef;

/* Report information: hit times, miss time, read/write access times, hit ratio */
//L1 Data Cache
typedef struct {
    uint32_t Data_Hit;
    uint32_t Data_Miss;
    uint32_t Write_Back;
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
int Hit_Show = 0;
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
    //Check input traces and hit_show parameter
    printf("\033[32m==============================================================================================================\033[0m\n");
    printf("\033[32m\t\t\t\t\033[4;1mINITIALIZATION:\033[0m\n");
    if (argc < 2)
    {
        printf("\033[31mERROR: Trace file name not found!\033[0m\n");
        exit(1);
    }
    else
    {
        trace_file_name = argv[1];
        printf("\033[32;4;1m1. Trace file name:\033[0m\033[32m %s\n\033[0m", trace_file_name);
    }
    if (argc > 2) Hit_Show = atoi(argv[2]);
    //Clear cache and stats
    printf("\033[32;4;1m2. Resetting all cache lines and stats...\033[0m\n");
    if (Reset_And_Clear_Cache()) printf("\033[32m\t   => DONE\033[0m\n");
    else
    {
        printf("\033[31mERROR: Cannot reset and clear cache!\033[0m\n");
        exit(1);
    }

    //Select report mode
    Mode = (unsigned int) Selection_Menu();
    //Read trace file    
    if ((fd = Open_Trace_File(trace_file_name))) printf("\033[32;4;1m4. Trace file is opened successfully!\033[0m\n");
    else printf("\033[31mERROR: Cannot open trace file!\033[0m\n");
    
    printf("\033[32m==============================================================================================================\033[0m\n");
    //Read and Run the Simulation
    if (Mode > 0) printf("\033[33m==============================================================================================================\033[0m\n");
    printf("\033[33m\t\t\t\t\033[4;1mMESSAGE BETWEEN L1 AND L2:\033[0m\n");
    if (Read_and_Run_Trace_File(fd) == false) printf("\033[31mERROR: Cannot read and simulate trace file!\033[0m\n");
    
    //FINISH MESSAGE
    printf("\033[32;1m\t\t\t\t\t\tTEST FINISHED!\033[0m\n");
    /* END Code */

    return 0;
}
/* END MAIN PROGRAM */

/*======================================================================*/

/* BEGIN User function */
bool Reset_And_Clear_Cache()
{
    bool OK = false;    
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

    return OK = true;    
}

unsigned int Selection_Menu()
{       
    unsigned int mode = 3;

    printf("\033[32;4;1m3. Display Mode Selection:                                                             .\n");
    printf("|  Mode  |                                Description                                  |\n");
    printf("|   0    |                         Cache content & Statistics                          |\n");
    printf("|   1    |                 Mode 0 information & message between L1/L2                  |\n"); 
    do {
        printf("\033[0m\033[32m\tPLEASE SELECT A MODE: ");
        scanf("%u", &mode);
        printf("\tTHE ENTERED MODE IS: %u\033[0m\n", mode);
    } while (mode > 1);    
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
        sscanf(one_trace_line, "%u %x %c", &tmp_operation, &address, comments);        
        if (one_trace_line[0]=='#'||!strcmp(one_trace_line, "\n")||!strcmp(one_trace_line, " ")||!strcmp(one_trace_line, "/")||!strcmp(one_trace_line, "*")||!strcmp(one_trace_line, "=")||(tmp_operation > 9)) __asm__("nop");
        else
        {            
            switch (tmp_operation)
            {
            case READ:                
                Data_Cache_Read(address);                          
                break;
            
            case WRITE:
                Data_Cache_Write(address);
                break;
            
            case FETCH:
                Instruction_Cache_Fetch(address);                             
                break;

            case EVICT:                
                L2_Evict_Command_to_L1(address);                                
                break;                

            case RESET_AND_CLEAR:
                Reset_And_Clear_Cache();                                              
                break;

            case PRINT_LOG:                
                Print_Content_And_State();
                break;

            default:
                printf("\033[1;31mERROR: Ivalid operation!\033[1;0m\n");
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

    Data_Stats_Report.Data_Read_Access++;
    Selected_Cache_Way = Data_Match_Find(Tag, Set);        
    if (Selected_Cache_Way > -1)
    {
        if (Data_Cache[Selected_Cache_Way][Set].Valid)
        {
            Data_Stats_Report.Data_Hit++;
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = Data_Cache[Selected_Cache_Way][Set].Valid;
            Data_Cache[Selected_Cache_Way][Set].Dirty = Data_Cache[Selected_Cache_Way][Set].Dirty;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            if ((Mode > 0) && (Hit_Show == 1)) printf("\033[33m[READ ACCESS %6u] L1(DATA)  READ HIT <0x%08x>\033[0m\n", Data_Stats_Report.Data_Read_Access, address);
        }        
        else
        {
            if (Mode > 0) printf("\033[33;4m[READ_ ACCESS %6u] L1(DATA)  READ MISS  - Read from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Read_Access, address);            
            Data_Stats_Report.Data_Miss++;
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = 1;
            Data_Cache[Selected_Cache_Way][Set].Dirty = Data_Cache[Selected_Cache_Way][Set].Dirty;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
        }                
    }
    else
    {        
        Data_Stats_Report.Data_Miss++;
        uint16_t Tag_Mask = pow(2, TAG_BIT);
        for (uint8_t i = 0; (Selected_Cache_Way<0) && (i < DATA_WAYS); i++)
        {            
            if (Data_Cache[i][Set].tag == Tag_Mask)
            {
                Selected_Cache_Way = i;
                Empty_Flag = 1;
            }            
        }
        if (Selected_Cache_Way > -1)
        {
            if (Mode > 0)
            {
                printf("\033[33;4m[READ_ ACCESS %6u] L1(DATA)  READ MISS  - Read from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Read_Access, address);
            }   
            Data_Cache[Selected_Cache_Way][Set].tag = Tag;
            Data_Cache[Selected_Cache_Way][Set].set = Set;
            Data_Cache[Selected_Cache_Way][Set].Valid = 1;
            Data_Cache[Selected_Cache_Way][Set].Dirty = Data_Cache[Selected_Cache_Way][Set].Dirty;
            Data_Cache[Selected_Cache_Way][Set].address = address;
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);         
        }  
        else
        {      
            for (uint8_t i = 0; i < DATA_WAYS; i++)
            {
                if (Data_Cache[i][Set].Valid == 0) 
                {
                    Selected_Cache_Way = i;
                }
            }
            if (Selected_Cache_Way < 0) 
            {
                Selected_Cache_Way = Data_LRU_Smallest_Find(Set);            
                if (Selected_Cache_Way > -1)
                {
                    if (0 == Data_Cache[Selected_Cache_Way][Set].Dirty)
                    {                      
                        if (Mode > 0)
                        {                            
                            printf("\033[33;4m[READ_ ACCESS %6u] L1(DATA)  READ MISS  - L1 evict <0x%08x> - Read from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Read_Access, Data_Cache[Selected_Cache_Way][Set].address, address);              
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
                        if (Mode > 0) printf("\033[33;4m[READ_ ACCESS %6u] L1(DATA)  READ MISS  - Write to L2 <0x%08x> - Read from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Read_Access, Data_Cache[Selected_Cache_Way][Set].address, address);                                    
                        Data_Stats_Report.Write_Back++;                        
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
            else
            {
                if (Mode > 0) printf("\033[33;4m[READ_ ACCESS %6u] L1(DATA)  READ MISS  - L1 evict <0x%08x> - Read from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Read_Access, Data_Cache[Selected_Cache_Way][Set].address, address);                
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

    Data_Stats_Report.Data_Write_Access++;
    Selected_Cache_Way = Data_Match_Find(Tag, Set);        
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
            Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            if ((Mode > 0) && (Hit_Show == 1)) printf("\033[33m[WRITE ACCESS %6u] L1(DATA)  WRITE HIT <0x%08x>\033[0m\n", Data_Stats_Report.Data_Write_Access, address);            
        }
        else
        {
            if (Mode > 0) printf("\033[33;4m[WRITE ACCESS %6u] L1(DATA)  WRITE MISS - Read for Ownership from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Write_Access, address);            
            Data_Stats_Report.Data_Miss++;
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
        Data_Stats_Report.Data_Miss++;
        uint16_t Tag_Mask = pow(2, TAG_BIT);

        for (uint8_t i = 0; (Selected_Cache_Way<0) && (i < DATA_WAYS); i++)
        {            
            if (Data_Cache[i][Set].tag == Tag_Mask)
            {
                Selected_Cache_Way = i;
                Empty_Flag = 1;
            }            
        }
        if (Selected_Cache_Way > -1)
        {
            if (Mode > 0)
            {
                printf("\033[33;4m[WRITE ACCESS %6u] L1(DATA)  WRITE MISS - Read for Ownership from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Write_Access, address);          
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
            for (uint8_t i = 0; i < DATA_WAYS; i++)
            {
                if (Data_Cache[i][Set].Valid == 0) 
                {
                    Selected_Cache_Way = i;
                }
            }
            if (Selected_Cache_Way < 0) 
            {
                Selected_Cache_Way = Data_LRU_Smallest_Find(Set);
                if (Selected_Cache_Way > -1)
                {
                    if (0 == Data_Cache[Selected_Cache_Way][Set].Dirty)
                    {                 
                        if (Mode > 0) printf("\033[33;4m[WRITE ACCESS %6u] L1(DATA)  WRITE MISS - L1 evict <0x%08x> - Read for Ownership from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Write_Access, Data_Cache[Selected_Cache_Way][Set].address, address);                                                        
                        Data_Cache[Selected_Cache_Way][Set].tag = Tag;
                        Data_Cache[Selected_Cache_Way][Set].set = Set;
                        Data_Cache[Selected_Cache_Way][Set].Valid = 1;
                        Data_Cache[Selected_Cache_Way][Set].Dirty = 1;
                        Data_Cache[Selected_Cache_Way][Set].address = address;
                        Data_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                    }
                    else
                    {
                        if (Mode > 0) printf("\033[33;4m[WRITE ACCESS %6u] L1(DATA)  WRITE MISS - Write to L2 <0x%x> - Read for Ownership from L2 <0x%08x>\033[0m\n", Data_Stats_Report.Data_Write_Access, Data_Cache[Selected_Cache_Way][Set].address, address);                                                            
                        Data_Stats_Report.Write_Back++;
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
                    printf("\033[31;4mERROR: WRITE - THE LRU DATA IS CORRUPTED!\033[0m\n");
                    return true;
                }                                                
            }
            else
            {
                if (Mode > 0) printf("\033[33;4m[WRITE ACCESS %6u] L1(DATA)  WRITE MISS - L1 evict <0x%08x> - Read for Ownership from L2 <0x%08x>)\033[0m\n", Data_Stats_Report.Data_Write_Access, Data_Cache[Selected_Cache_Way][Set].address, address);                
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

    Instr_Stats_Report.Instruction_Read_Access++;
    Selected_Cache_Way = Instruction_Match_Find(Tag, Set);
    if (Selected_Cache_Way > -1)
    {
        if (Instr_Cache[Selected_Cache_Way][Set].Valid)
        {
            Instr_Stats_Report.Instruction_Hit++;
            Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
            Instr_Cache[Selected_Cache_Way][Set].set = Set;
            Instr_Cache[Selected_Cache_Way][Set].Valid = Instr_Cache[Selected_Cache_Way][Set].Valid;
            Instr_Cache[Selected_Cache_Way][Set].Dirty = Instr_Cache[Selected_Cache_Way][Set].Dirty;
            Instr_Cache[Selected_Cache_Way][Set].address = address;
            Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
            if ((Mode > 0) && (Hit_Show == 1)) printf("\033[33m[READ_ ACCESS %6u] L1(INSTR) READ HIT <0x%08x>\033[0m\n",  Instr_Stats_Report.Instruction_Read_Access, address);
        }
        else
        {
            if (Mode > 0) printf("\033[33;4m[READ_ ACCESS %6u] L1(INSTR) READ MISS  - Read from L2 <0x%08x>\033[0m\n", Instr_Stats_Report.Instruction_Read_Access, address);            
            Instr_Stats_Report.Instruction_Miss++;
            Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
            Instr_Cache[Selected_Cache_Way][Set].set = Set;
            Instr_Cache[Selected_Cache_Way][Set].Valid = 1;
            Instr_Cache[Selected_Cache_Way][Set].Dirty = Instr_Cache[Selected_Cache_Way][Set].Dirty;
            Instr_Cache[Selected_Cache_Way][Set].address = address;
            Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
        }
    }
    else
    {
        Instr_Stats_Report.Instruction_Miss++;
        uint16_t Tag_Mask = pow(2, TAG_BIT);
        for (uint8_t i = 0; (Selected_Cache_Way<0) && (i < INSTR_WAYS); i++)
        {            
            if (Instr_Cache[i][Set].tag == Tag_Mask)
            {
                Selected_Cache_Way = i;
                Empty_Flag = 1;
            }            
        }
        if (Selected_Cache_Way > -1)
        {
            if (Mode >= 1) printf("\033[33;4m[READ_ ACCESS %6u] L1(INSTR) READ MISS  - Read from L2 <0x%08x>\033[0m\n", Instr_Stats_Report.Instruction_Read_Access, address);              
            Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
            Instr_Cache[Selected_Cache_Way][Set].set = Set;
            Instr_Cache[Selected_Cache_Way][Set].Valid = 1;
            Instr_Cache[Selected_Cache_Way][Set].Dirty = Instr_Cache[Selected_Cache_Way][Set].Dirty;
            Instr_Cache[Selected_Cache_Way][Set].address = address;
            Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);         
        }
        else
        {
            for (uint8_t i = 0; i < INSTR_WAYS; i++)
            {
                if (Instr_Cache[i][Set].Valid == 0)  Selected_Cache_Way = i;                
            }
            if (Selected_Cache_Way < 0)
            {
                Selected_Cache_Way = Instruction_LRU_Smallest_Find(Set);
                if (Selected_Cache_Way > -1)
                {
                    if (0 == Instr_Cache[Selected_Cache_Way][Set].Dirty)
                    {
                        if (Mode > 0) printf("\033[33;4m[READ_ ACCESS %6u] L1(INSTR) READ MISS  - L1 evict <0x%08x> - Read from L2 <0x%08x>\033[0m\n", Instr_Stats_Report.Instruction_Read_Access, Data_Cache[Selected_Cache_Way][Set].address, address);                                                    
                        Instr_Cache[Selected_Cache_Way][Set].tag = Tag;
                        Instr_Cache[Selected_Cache_Way][Set].set = Set;
                        Instr_Cache[Selected_Cache_Way][Set].Valid = 1;
                        Instr_Cache[Selected_Cache_Way][Set].Dirty = 0;
                        Instr_Cache[Selected_Cache_Way][Set].address = address;
                        Instruction_LRU_State_Update(Selected_Cache_Way, Set, Empty_Flag);
                    }
                    else printf("ERROR: Dirty is set to 1 in instruction cache!\n");                    
                }
                else
                {
                    printf("ERROR: READ - THE LRU INSTRUCTION IS CORRUPTED!\n");
                    return true;
                }                
            }
            else
            {
                if (Mode > 0) printf("\033[33;4m[READ_ ACCESS %6u] L1(INSTR) READ MISS  - L1 evict <0x%08x> - Read from L2 <0x%08x>\033[0m\n", Instr_Stats_Report.Instruction_Read_Access, Data_Cache[Selected_Cache_Way][Set].address, address);                
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
    uint16_t Tag = address >> (SET_BIT + BYTE_BIT);
    uint16_t Set = (address & SET_MASK) >> BYTE_BIT;
    bool Match_Line = false;
    bool Search_Instructions = false;

    for (uint8_t i = 0; i < DATA_WAYS; i++)
    {
        if (Data_Cache[i][Set].tag == Tag)
        {
            Match_Line = true;
            if (Data_Cache[i][Set].Valid == 1)
            {
                if (Data_Cache[i][Set].Dirty == 0)
                {
                    Data_Cache[i][Set].tag = Tag;
                    Data_Cache[i][Set].set = Set;
                    Data_Cache[i][Set].Valid = 0;
                    Data_Cache[i][Set].address = address;
                }
                else
                {
                    if (Mode > 0) printf("\033[33;4mEVICTION FROM L2 - Write to L2 <0x%08x>\033[0m\n", address);                    
                    Data_Cache[i][Set].tag = Tag;
                    Data_Cache[i][Set].set = Set;
                    Data_Cache[i][Set].Valid = 0;
                    Data_Cache[i][Set].address = address;                    
                }                                
            }
            else
            {
                Data_Cache[i][Set].tag = Tag;
                Data_Cache[i][Set].set = Set;                                
                Data_Cache[i][Set].address = address;
            }                        
        }
        else
        {
            if ((i >= 3) && (Match_Line == false)) Search_Instructions = true;
                //printf("ERROR: LINE NOT FOUND IN L1!\n");
                //eturn true;           
        }
    }
    if (Search_Instructions == true)
    {
        for (uint8_t i = 0; i < INSTR_WAYS; i++)
        {
            if (Instr_Cache[i][Set].tag == Tag)
            {
                Match_Line = true;
                if (Instr_Cache[i][Set].Valid == 1)
                {
                    Instr_Cache[i][Set].tag = Tag;
                    Instr_Cache[i][Set].set = Set;
                    Instr_Cache[i][Set].Valid = 0;
                    Instr_Cache[i][Set].address = address;
                }
                else
                {
                    Instr_Cache[i][Set].tag = Tag;
                    Instr_Cache[i][Set].set = Set;                                
                    Instr_Cache[i][Set].address = address;
                }                
            }
            else
            {
                if ((i >= 1) && (Match_Line == false))
                {
                    printf("ERROR: LINE NOT FOUND IN L1!\n");
                    return true;
                }
                
            }                        
        }        
    }
    return false;    
}

bool Print_Content_And_State()
{
    uint8_t Valid_in_Set = 0;
    uint16_t Sets_Number = pow(2, SET_BIT);

    Data_Stats_Report.Data_Hit_Ratio = (float)((Data_Stats_Report.Data_Hit*1.0)/(Data_Stats_Report.Data_Miss + Data_Stats_Report.Data_Hit));
    Instr_Stats_Report.Instr_Hit_Ratio = (float)((Instr_Stats_Report.Instruction_Hit*1.0)/(Instr_Stats_Report.Instruction_Miss + Instr_Stats_Report.Instruction_Hit));    
    if (Mode > 0) printf("\033[33m==============================================================================================================\033[0m\n");
    printf("\033[36m==============================================================================================================\033[0m\n");
    printf("\033[36m\t\t\t\t\033[4;1mL1 CACHE SUMMARY AND STATISTICS:\033[0m\n");
    //Data Cache Information
    printf("\033[36m\033[4;1m1. DATA CACHE CONTENT:\n\033[0m\n");
    for (uint16_t i = 0; i < Sets_Number; i++)
    {
        for (uint8_t j = 0; j < DATA_WAYS; j++)
        {
            if (Data_Cache[j][i].Valid == 1)
            {
                if (Valid_in_Set == 0)
                {
                    printf("\033[36m\033[4mSet Index: %u\033[0m\n", i);
                    Valid_in_Set = 1;
                }
                printf("\033[36mWay Index: %u || Address: 0x%08x || Tag: %04u || Set: %05u || LRU: %1d || Valid: %u || Dirty: %d\033[0m\n", 
                j, Data_Cache[j][i].address, Data_Cache[j][i].tag, Data_Cache[j][i].set, Data_Cache[j][i].LRU_State, Data_Cache[j][i].Valid, Data_Cache[j][i].Dirty);
            }            
        }
        Valid_in_Set = 0;
    }
    printf("\033[36m--------------------------------------------------------------------------------------------------------------\033[0m\n");

    //Instruction Cache Informatinon
    printf("\033[36m\033[4;1m2. INSTRUCTION CACHE CONTENT:\n\033[0m\n");
    for (uint16_t i = 0; i < Sets_Number; i++)
    {
        for (uint8_t j = 0; j < INSTR_WAYS; j++)
        {
            if (Instr_Cache[j][i].Valid == 1)
            {
                if (Valid_in_Set == 0)
                {
                    printf("\033[36m\033[4mSet Index: %u\033[0m\n", i);
                    Valid_in_Set = 1;
                }
                printf("\033[36mWay Index: %u || Address: 0x%08x || Tag: %04u || Set: %05u || LRU: %1d || Valid: %u\033[0m\n", 
                j, Instr_Cache[j][i].address, Instr_Cache[j][i].tag, Instr_Cache[j][i].set, Instr_Cache[j][i].LRU_State, Instr_Cache[j][i].Valid);
            }            
        }
        Valid_in_Set = 0;
    }
    printf("\033[36m--------------------------------------------------------------------------------------------------------------\033[0m\n");

    //Statistics
    printf("\033[36m\033[4;1m3. L1 CACHE STATISTICS:\n\033[0m\n");
    printf("\033[36m\033[1ma. DATA CACHE:\033[0m\n");
    if (Data_Stats_Report.Data_Miss == 0)
    {
        printf("\t\033[36mNo operation was executed on Data Cache!\033[0m\n");        
    }
    else
    {
        printf("\033[36m\t+Data Cache Read Accesses: %u\n\t+Data Cache Write Accesses: %u\n\t+Data Cache Write Backs: %u\n\t+Data Cache Hits: %u\n\t+Data Cache Misses: %u\n\t+Data Cache Hit Ratio: %1.4f\n\033[0m\n", 
        Data_Stats_Report.Data_Read_Access, Data_Stats_Report.Data_Write_Access, Data_Stats_Report.Write_Back, Data_Stats_Report.Data_Hit, Data_Stats_Report.Data_Miss, Data_Stats_Report.Data_Hit_Ratio);
    }
    printf("\033[36m\033[1mb. INSTRUCTION CACHE:\033[0m\n");
    if (Instr_Stats_Report.Instruction_Miss == 0)
    {
        printf("\033[36m\tNo operation was executed on Instruction Cache!\033[0m\n");        
    }
    else
    {
        printf("\033[36m\t+Instruction Cache Read Accesses: %u\n\t+Instruction Cache Write Accesses: %u\n\t+Instruction Cache Hits: %u\n\t+Instruction Cache Misses: %u\n\t+Instruction Cache Hit Ratio: %1.4f\n\033[0m\n", 
        Instr_Stats_Report.Instruction_Read_Access, Instr_Stats_Report.Instruction_Write_Access, Instr_Stats_Report.Instruction_Hit, Instr_Stats_Report.Instruction_Miss, Instr_Stats_Report.Instr_Hit_Ratio);
    }
    printf("\033[36m==============================================================================================================\033[0m\n");
    return false;
}

//Support functions
int Data_Match_Find(unsigned int Input_Tag, unsigned int Input_Set)
{        
    for (uint8_t i = 0; i < DATA_WAYS; i++)
    {
        if (Data_Cache[i][Input_Set].tag == Input_Tag) return i;               
    }
    return -1;
}

int Instruction_Match_Find(unsigned int Input_Tag, unsigned int Input_Set)
{
    for (uint8_t i = 0; i < INSTR_WAYS; i++)
    {
        if (Instr_Cache[i][Input_Set].tag == Input_Tag) return i;                
    }
    return -1;
}

void Data_LRU_State_Update(unsigned int Set_Way, unsigned int Cache_Set, uint8_t Empty_Flag)
{
    uint8_t LRU_Current_State = Data_Cache[Set_Way][Cache_Set].LRU_State;
    if (0 == Empty_Flag)
    {        
        for (uint8_t i = 0; i < DATA_LRU; i++)
        {
            if (LRU_Current_State > Data_Cache[i][Cache_Set].LRU_State) __asm__("nop");
            else --Data_Cache[i][Cache_Set].LRU_State;                       
        }        
    }
    else              
    {
        for (uint8_t i = 0; i < Set_Way; i++)
        {
            if (Data_Cache[i][Cache_Set].LRU_State < 4) --Data_Cache[i][Cache_Set].LRU_State;
            else printf("\033[1;31mERROR: LRU DATA CORRUPTED\033[1;0m\n");                                     
        }        
    }
    Data_Cache[Set_Way][Cache_Set].LRU_State = 3; 
}

void Instruction_LRU_State_Update(unsigned int Set_Way, unsigned int Cache_Set, uint8_t Empty_Flag)
{
    uint8_t LRU_Current_State = Instr_Cache[Set_Way][Cache_Set].LRU_State;

    if (0 == Empty_Flag) 
    {        
        for (uint8_t i = 0; i < INSTR_LRU; i++)
        {
            if (LRU_Current_State > Instr_Cache[i][Cache_Set].LRU_State) __asm__("nop");
            else --Instr_Cache[i][Cache_Set].LRU_State;                      
        }        
    }
    else for (uint8_t i = 0; i < Set_Way; i++) --Instr_Cache[i][Cache_Set].LRU_State;                    
    Instr_Cache[Set_Way][Cache_Set].LRU_State = 1;
}

int Data_LRU_Smallest_Find(uint16_t Set_Index)
{
    for (uint8_t i = 0; i < DATA_WAYS; i++)
    {
        if (Data_Cache[i][Set_Index].LRU_State == 0) return i;               
    }
    return -1;
}

int Instruction_LRU_Smallest_Find(uint16_t Set_Index)
{
    for (uint8_t i = 0; i < INSTR_WAYS; i++)
    {
        if (Instr_Cache[i][Set_Index].LRU_State == 0) return i;              
    }
    return -1;
}
/* END User function */