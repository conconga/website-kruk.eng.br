//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
/*
Author:      Luciano Augusto Kruk
Version:     2008
Description: Too old, and in Portuguese. I promise I will translate it once
            once I need it again!
*/

# include "msp430x20x3.h"
//# include <io430x20x3.h>

# define      P10     0x01
# define      P11     0x02
# define      P12     0x04
# define      P13     0x08
# define      P14     0x10
# define      P15     0x20
# define      P16     0x40
# define      P17     0x80

# define      LED             P14
# define      CARTAO_CS       P10
# define      CARTAO_CLK      P15
# define      CARTAO_DI       P16
# define      CARTAO_DO       P17

# define      ATIVAR_ENVIO()      USICNT = 8; USICTL1 |= 0x10; // macro;
# define      DESATIVAR_ENVIO()   USICTL1 &= ~0x10; // macro;

# define      CMD(a)                          ((a)|0x40) // macro;
# define      CRC(a)                          (((a)<<1)|0x01) // macro;
# define      CMD_NULL                        0xFF
# define      CMD_GO_IDLE_STATE               0
# define      CMD_SEND_OP_COND                1
# define      CMD_SEND_CSD                    9
# define      CMD_READ_SINGLE                 17
# define      CMD_WRITE_SINGLE                24
# define      CMD_ERASE_START_ADD             32
# define      CMD_ERASE_STOP_ADD              33
# define      CMD_ERASE                       38
# define      CMD_READ_OCR                    58

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo

struct stBUFFER
{
  unsigned char             *pucBuffer;
  volatile unsigned char    ucContador;
};

struct stCSD
{
  unsigned int      BLOCK_LEN; // bytes em cada bloco;
  unsigned int      C_SIZE;
  unsigned int      C_SIZE_MULT;
  unsigned long int BLOCK_NR; // numero total de blocos;
  unsigned int      SECTOR_SIZE; // numero de blocos em um setor (unidade de apagamento);
//  unsigned long int uliCapacidade; // capacidade do cartao, em bytes;
};

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo

__interrupt void          taifg_A(void);
__interrupt void          usiifg(void);
unsigned char             MEM_Enviar (unsigned char *puc, unsigned char nbytes);
void                      MEM_Aguardar (void);
unsigned char             MEM_AguardarResposta (
                                                unsigned char nBytes,
                                                unsigned char to,
                                                unsigned char *puc);
void                      MEM_ApagarSetor(unsigned long int setor);
void                      MEM_Leitura(unsigned long int setor, unsigned int bloco);


struct stBUFFER             g_stEnviar;
struct stBUFFER             g_stReceber;
unsigned int                g_ui01;
unsigned long int           g_uli01;
unsigned char               *g_puc01;
volatile unsigned char      g_ucByteRecebido;
unsigned char               g_pucComando[6]; // buffer com comando para o cartao;
unsigned char               g_pucResposta[30]; // buffer com resposta do cartao;
struct stCSD                g_stCSD;

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
main()
{
  // stop watchdog timer
  WDTCTL = WDTPW + WDTHOLD;

  // iniciando algumas variaveis:

  // configuracoes de portas:
  P1OUT = CARTAO_CS;
  P1DIR = LED + CARTAO_CS;// + CARTAO_CLK;// + CARTAO_DI;

  // configuracao da serial SPI:
  USICTL1 = 0x00; // SPI;
  USICKCTL = 0x08; // SMCLK/1;
  USICTL0 = 0xEE; // SDI,SDO,SCLK,MSB,master;

  // habilitando interrupcoes:
  _BIS_SR(GIE);

  // alguns clocks para comecar:
  g_pucComando[0] = CMD(CMD_NULL);
  for (g_ui01 = 10; g_ui01 > 0; g_ui01--)
    MEM_Enviar(g_pucComando, 1);

  // iniciar SPI mode:
  g_pucComando[0] = CMD(CMD_GO_IDLE_STATE);
  g_pucComando[1] = g_pucComando[2] = g_pucComando[3] = g_pucComando[4] = 0;
  g_pucComando[5] = CRC(0x4A);
  MEM_Enviar(g_pucComando, 6);
  MEM_AguardarResposta(2,10,g_pucResposta);
  if (g_pucResposta[0] != 0x01)
    while (1); // erro iniciando o cartao!

  // leitura do registro OCR:
  g_pucComando[0] = CMD(CMD_READ_OCR);
  g_pucComando[5] = CRC(0); // enquanto o CRC estiver desabilitado, nao serah
                            // mais necessario atualizar este valor;
  MEM_Enviar(g_pucComando, 6);
  MEM_AguardarResposta(10,10,g_pucResposta);

  // colocar o cartao em condicao operacional:
  while (g_pucResposta[0] & 0x01)
  {
    g_pucComando[0] = CMD(CMD_SEND_OP_COND); // AguardarResposta() muda este valor;
    MEM_Enviar(g_pucComando, 6);
    MEM_AguardarResposta(6,10,g_pucResposta);
  };

  // leitura do registro CSD:
  g_pucComando[0] = CMD(CMD_SEND_CSD);
  MEM_Enviar(g_pucComando, 6);
  MEM_AguardarResposta(30,10,g_pucResposta);
  // Esta resposta deve conter R1, n vezes 0xFF, e o data token, com
  // 0xFE no comeco, 16 bytes de CSD e 2 bytes de CRC no final.
  // Uma outra maneira de fazer a mesma coisa:
  g_pucComando[0] = CMD(CMD_SEND_CSD);
  MEM_Enviar(g_pucComando, 6);
  MEM_AguardarResposta(2,10,g_pucResposta);
  MEM_AguardarResposta(30,10,g_pucResposta); // o byte [0] deve ser 0xFE;

  // informacoes criticas do cartao:
  g_stCSD.BLOCK_LEN = 1 << (g_pucResposta[6] & 0x0F);
  g_stCSD.C_SIZE_MULT = ((g_pucResposta[10] & 0x03)<<1) + ((g_pucResposta[11] & 0x80)>>7);
  g_stCSD.C_SIZE = ((g_pucResposta[7] & 0x03)<<10) + (g_pucResposta[8]<<2) +
    ((g_pucResposta[9] & 0xC0) >> 6);
  g_stCSD.BLOCK_NR = 1 << (g_stCSD.C_SIZE_MULT + 2);
  g_stCSD.BLOCK_NR *= g_stCSD.C_SIZE + 1;
  g_stCSD.SECTOR_SIZE = ((g_pucResposta[11] & 0x3F)<<1) + ((g_pucResposta[12] & 0x80)>>7) + 1;
/*
  g_stCSD.uliCapacidade = ((unsigned long int) g_stCSD.BLOCK_NR) *
    ((unsigned long int) g_stCSD.BLOCK_LEN);// [B] memory_capacity;
*/

  # define  SETOR 1

  // apagar um setor:
  MEM_ApagarSetor(SETOR);

  // leitura do primeiro bloco do setor:
  MEM_Leitura(SETOR,0);
  MEM_AguardarResposta(30,10,g_pucResposta);
  while (!MEM_AguardarResposta(1,10,g_pucResposta)) {}; // aguardar fim do bloco;

  // escrever no primeiro bloco do setor:
  g_uli01 = SETOR * g_stCSD.SECTOR_SIZE * g_stCSD.BLOCK_LEN;
  g_puc01 = (unsigned char *) &g_uli01;
  g_puc01 += 3;
  g_pucComando[1] = *g_puc01--;
  g_pucComando[2] = *g_puc01--;
  g_pucComando[3] = *g_puc01--;
  g_pucComando[4] = *g_puc01;
  g_pucComando[0] = CMD(CMD_WRITE_SINGLE);
  MEM_Enviar(g_pucComando, 6);
  MEM_AguardarResposta(30,10,g_pucResposta);
  g_pucComando[0] = 0xFE; // data token;
  g_pucComando[1] = 0;
  g_ui01 = g_stCSD.BLOCK_LEN - 1;
  MEM_Enviar(g_pucComando, 2);
  while (g_ui01--)
  {
    MEM_Aguardar();
    g_pucComando[1]++;
    MEM_Enviar(&g_pucComando[1], 1);
  };
  MEM_Aguardar();
  MEM_Enviar(g_pucComando, 2);
  while (!MEM_AguardarResposta(30,10,g_pucResposta)) {}; // aguardar fim do bloco;

  // leitura do primeiro bloco do setor:
  MEM_Leitura(SETOR,0);
  MEM_AguardarResposta(30,10,g_pucResposta);
  while (!MEM_AguardarResposta(30,10,g_pucResposta)) {}; // aguardar fim do bloco;

  // apagar o setor recem-gravado;
  MEM_ApagarSetor(SETOR);

  // leitura do primeiro bloco do setor:
  MEM_Leitura(SETOR,0);
  MEM_AguardarResposta(30,10,g_pucResposta);
  while (!MEM_AguardarResposta(30,10,g_pucResposta)) {}; // aguardar fim do bloco;

  # undef   SETOR

  // principal:
  P1OUT |= LED;
  while(1)
  {
  }
}

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
// Iniciar transmissao;
// Esta rotina preenche os campos faltantes na estrutura stBUFFER e
// inicia o envio do primeiro byte. Via interrupcao, todos os bytes
// serao enviados.
// Se esta funcao for chamada antes do envio do ultimo byte, a funcao
// retorna erro.
//
// Entradas:
//  puc       :   endereco do buffer para ser enviado;
//  nbytes    :   numero de bytes para enviar;
// Retorno:
//  0 : sucesso;
//  1 : erro;
//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo

unsigned char MEM_Enviar(unsigned char *puc, unsigned char nbytes)
{
  if (g_stEnviar.ucContador)
    return (1);

  g_stEnviar.ucContador = nbytes;
  g_stEnviar.pucBuffer = puc;
  P1OUT &= ~CARTAO_CS;
  USISRL = *g_stEnviar.pucBuffer;
  ATIVAR_ENVIO();

  return (0);
};

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
// Aguardar fim de transmissao atual;
// Esta rotina interrompe a execucao normal do programa e aguarda o fim
// de qualquer transmissao atual via SPI antes de retornar.
//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo

void MEM_Aguardar (void)
{
  while (g_stEnviar.ucContador);
};

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
// Aguardar fim de transmissao atual, e resposta do cartao.
// Esta rotina interrompe a execucao normal do programa, aguarda o fim
// de qualquer transmissao atual via SPI, e espera resposta do cartao
// para ultimo comando enviado, antes de retornar.
//
// Entradas:
//  nBytes    :   num. de bytes para receber;
//  to        :   contador de "timeout", indicando num. max. de bytes para
//                esperar (1 <= to <= 254);
//  puc       :   endereco aonde serao guardados os dados recebidos;
//
// Saidas:
//  0  :  sucesso;
//  1  :  "timeout"
//
// O ultimo byte recebido fica disponivel na variavel global:
// unsigned char   g_ucByteRecebido;
//
// Por uma necessidade de 8 clocks para o cartao concluir suas operacoes,
// recomenda-se que o valor de nBytes seja uma unidade maior do que a
// necessaria.
//
// IMPORTANTE:
// Esta rotina muda o valor da variavel global:
// unsinged char    g_pucComando[];
//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo

unsigned char MEM_AguardarResposta (
                           unsigned char nBytes,
                           unsigned char to,
                           unsigned char *puc)
{
  // declaracoes locais:

  g_stReceber.ucContador = nBytes;
  g_stReceber.pucBuffer = puc;
  while (g_stEnviar.ucContador);
  while (to--)
  {
    g_pucComando[0] = CMD(CMD_NULL);
    MEM_Enviar(g_pucComando, 1);
    while (g_stEnviar.ucContador);
    if ((g_ucByteRecebido != 0xFF) || (g_stReceber.ucContador < nBytes))
    {
      *g_stReceber.pucBuffer++ = g_ucByteRecebido;
      to = 1;
      if (!(--g_stReceber.ucContador))
        return (0);
    };
  };
  return (1); // timeout;
};

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
// Apagar um setor da memoria;
// Esta rotina apaga um setor da memoria. O numero total de setores eh
// dado por ( g_stCSD.BLOCK_NR / g_stCSD.SECTOR_SIZE ).
// Entradas:
//  setor        :  indice (zero-based) do setor para apagar;
//
// IMPORTANTE:
// Esta rotina altera o valor da variavel global:
// unsigned char   g_pucComando[];
//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo

void MEM_ApagarSetor(unsigned long int setor)
{
  // declaracoes locais:
  unsigned long int   uliEndereco;
  unsigned char       *puc;

  uliEndereco = setor * g_stCSD.SECTOR_SIZE * g_stCSD.BLOCK_LEN;
  puc = (unsigned char *) &uliEndereco;
  puc += 3;
  g_pucComando[1] = *puc--;
  g_pucComando[2] = *puc--;
  g_pucComando[3] = *puc--;
  g_pucComando[4] = *puc;
  g_pucComando[0] = CMD(CMD_ERASE_START_ADD);
  puc = (unsigned char *) &uliEndereco;
  MEM_Enviar(g_pucComando, 6);
  while (!MEM_AguardarResposta(1,10,puc));

  uliEndereco = ((setor+1) * g_stCSD.SECTOR_SIZE * g_stCSD.BLOCK_LEN) - 1;
  puc = (unsigned char *) &uliEndereco;
  puc += 3;
  g_pucComando[1] = *puc--;
  g_pucComando[2] = *puc--;
  g_pucComando[3] = *puc--;
  g_pucComando[4] = *puc;
  g_pucComando[0] = CMD(CMD_ERASE_STOP_ADD);
  puc = (unsigned char *) &uliEndereco;
  MEM_Enviar(g_pucComando, 6);
  while (!MEM_AguardarResposta(1,10,g_pucResposta));

  g_pucComando[0] = CMD(CMD_ERASE);
  MEM_Enviar(g_pucComando, 6);
  MEM_AguardarResposta(1,10,puc); // aguarda resposta R1;
  while (!MEM_AguardarResposta(1,10,puc)) {}; // espera DataOut == H;
}

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
// Comandar a Leitura de Blocos;
// Esta rotina comanda a leitura de blocos, mas NAO realiza a leitura,
// pois o componente nao tem memoria para guardar todos os bytes de um
// bloco na RAM interna. Por isso, a leitura deve ser comandada externamente
// via rotina MEM_AguardarResposta().
// Entrada:
//  setor   :     indice (zero-based) do setor para ler;
//  bloco   :     indice (zero-based) do bloco para ler;
//
// IMPORTANTE:
// Esta rotina altera o valor da variavel global:
// unsigned char   g_pucComando[];
//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo

void MEM_Leitura(unsigned long int setor, unsigned int bloco)
{
  // declaracoes locais:
  unsigned long int   uliEndereco;
  unsigned char       *puc;

  uliEndereco = ((setor * g_stCSD.SECTOR_SIZE) + bloco) * g_stCSD.BLOCK_LEN;
  puc = (unsigned char *) &uliEndereco;
  puc += 3;
  g_pucComando[1] = *puc--;
  g_pucComando[2] = *puc--;
  g_pucComando[3] = *puc--;
  g_pucComando[4] = *puc;
  g_pucComando[0] = CMD(CMD_READ_SINGLE);
  MEM_Enviar(g_pucComando, 6);
};

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
# pragma vector=TIMERA0_VECTOR // TAIFG
__interrupt void taifg_A(void)
{
  // LED:
};

# pragma vector=USI_VECTOR // SPI
__interrupt void usiifg(void)
{
  g_ucByteRecebido = USISRL;

  // enviar proximo byte:
  if (--g_stEnviar.ucContador)
  {
    g_stEnviar.pucBuffer++;
    USISRL = *g_stEnviar.pucBuffer;
    USICNT = 8; // mais 8 bits;
  }
  else
  {
    P1OUT |= CARTAO_CS;
    DESATIVAR_ENVIO();
  };
};

//oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo.oo
