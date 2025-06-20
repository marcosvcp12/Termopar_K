#include <LiquidCrystal.h> //biblioteca para o LCD

// Configuração do LCD (conforme sua tabela)
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

//Definindo os pinos dos LEDs de saída
const int LED_VD = 6;//Verde
const int LED_AM = 7;//Amarelo
const int LED_VM = 8;//Vermelho

//Definindo o pino de entrada analógica conectado ao sinal amplificado do Termopar K
const int entrada = A5;

#define Vref 4.90 //Valor de acordo com a tensao no pino do arduino, será usada como referencia

//Conversão para bits, de acordo com o valor de referência
//Led Verde acende quando >= 1,6V && <= 2,4
#define VD_Min ((1.6*1023)/Vref)
#define VD_Max ((2.4*1023)/Vref)

//Led Amarelo acende quando >= 0V && <1,6 || > 2,4V && <= 4V
#define AM1_Max (((1.6*1023)/Vref)-1) //Máximo da primeira faixa amarela (1.6V)
#define AM2_Min (((2.4*1023)/Vref)+1) //Mínimo da segunda faixa amarela (2.4)
#define AM2_Max (((4*1023)/Vref)-1)//Máximo da segunda faixa amarela (4V)

//Definindo valores de histerese max e min
#define Histerese 10 //Valor de histerese em bits, como varia de 0 a 1023, temos que 5V/1023bit = 4,88mV/bit, então 10bit ≈ 50mV de histerese
#define VD_HistNeg (VD_Min - 10)
#define VD_HistPos (VD_Max + 10)
#define AM1_HistPos (AM1_Max + 10)
#define AM2_HistNeg (AM2_Min - 10) 
#define AM2_HistPos (AM2_Max + 10)

//Variável para armazenar o estado atual
int Estado_Anterior = 0;//definindo zero para nao receber valor de lixo

void setup() 
{
  //Configura os pinos dos LEDs como saída
  pinMode(LED_VM, OUTPUT);
  pinMode(LED_AM, OUTPUT);
  pinMode(LED_VD, OUTPUT);
  
  // Inicialização do LCD
  lcd.begin(16, 2);
  lcd.setCursor(0, 0);//Primeira linha, primeira coluna
  lcd.print("Iniciando...");//Mostra mensagem de inicialização na primeira linha
  lcd.setCursor(0, 1);//Segunda linha, primeira coluna
  lcd.print("Proj. Termopar K");//Nome do projeto na segunda linha
  delay(5000);//Aguarda 5 segundos para que o usuário leia a mensagem
  lcd.clear();//Limpa o display para começar a exibir os dados
  
  Serial.begin(9600);//Inicializa a comunicação serial a 9600 bps
}

void loop() 
{
  //Lê o valor analógico da entrada (0 a 1023 para 0 a 5V)
  int valor = analogRead(entrada);
  float tensao = valor * (Vref / 1023.0);//Conversão do valor lido pela entrada analógica do Arduino (de 0 a 1023) para uma tensão real em volts (0 a 5V)
  
  // Cálculo da temperatura com base na tensão medida
  //Faixa de tensão: 0V a 4V → Faixa de temperatura: 550°C a 1050°C
  float temperatura;
  if (tensao <= 0.0) //Se a tensão for 0V ou menor, assume-se a temperatura mínima (limite inferior)
  {
    temperatura = 550.0;
  } 
  else if (tensao <= 1.6) //Faixa 0V a 1.6V → temperatura de 550°C a 750°C (intervalo de 200°C)
  {
    temperatura = 550.0 + (tensao / 1.6) * 200.0;//Interpolação linear: (tensao / 1.6) * 200 + 550
  } 
  else if (tensao <= 2.4) //Faixa 1.6V a 2.4V → temperatura de 750°C a 850°C (intervalo de 100°C)
  {
    temperatura = 750.0 + ((tensao - 1.6) / 0.8) * 100.0;//Interpolação linear: ((tensao - 1.6) / 0.8) * 100 + 750
  } 
  else if (tensao <= 4.0)//Faixa 2.4V a 4.0V → temperatura de 850°C a 1050°C (intervalo de 200°C)
  {
    temperatura = 850.0 + ((tensao - 2.4) / 1.6) * 200.0;//Interpolação linear: ((tensao - 2.4) / 1.6) * 200 + 850
  } 
  else //Se a tensão for maior que 4V, assume-se a temperatura máxima (limite superior)
  {
    temperatura = 1050.0;
  }
  
  // Conversão para 4-20mA (0-4V corresponde a 4-20mA)
  float corrente = 4.0 + (tensao / 4.0) * 16.0;
  // A fórmula faz uma interpolação linear:
  // - 0V → 4mA
  // - 4V → 20mA
  // Cada 1V representa 4mA (16mA no total entre 4 e 20mA)
  if (corrente < 4.0) corrente = 4.0; //Garante que a corrente nunca seja menor que 4mA
  if (corrente > 20.0) corrente = 20.0; //Garante que a corrente nunca ultrapasse 20mA

  // Exibição OTIMIZADA no LCD
  lcd.setCursor(0, 0);//Linha superior
  lcd.print("V:");
  lcd.print(tensao, 2);
  lcd.print("V");
  lcd.print(" C:");
  lcd.print(corrente, 1);
  lcd.print("mA");

  lcd.setCursor(0, 1);//Linha inferior
  lcd.print("T:");
  lcd.print(temperatura, 2);
  lcd.write(byte(223));//símbolo °
  lcd.print("C");
  
  //Determina o estado atual com histerese
  int Estado_Atual;
  
  if (valor >= (VD_HistNeg) && valor <= (VD_HistPos))//Verificação da faixa verde com zona de histerese estendida
  {   
    if (valor >= VD_Min && valor <= VD_Max)//Caso 1: Dentro da faixa verde nominal (sem histerese)
    {
      Estado_Atual = 2; //Estado atual da faixa Verde
    } 
    else if (Estado_Anterior == 2)//Caso 2: Na zona de histerese superior/inferior com estado anterior verde
    {
      Estado_Atual = 2;//Mantém verde (aplica histerese positiva)
    } 
    else //Caso 3: Na zona de histerese mas estado anterior não era verde
    {
      Estado_Atual = 1; //Transição para amarelo (histerese negativa)
    }
  } 
  //Verificação das faixas amarelas (duas faixas distintas) com histerese
  else if ((valor >= 0 && valor <= (AM1_HistPos)) || (valor >= (AM2_HistNeg) && valor <= (AM2_HistPos)))
  {
    if ((valor >= 0 && valor <= AM1_Max) || (valor >= AM2_Min && valor <= AM2_Max))//Caso 1: Dentro das faixas amarelas nominais
    {
      Estado_Atual = 1;//Estado atual da faixa Amarelo
    } 
    else if (Estado_Anterior == 1)//Caso 2: Zona de histerese com estado anterior amarelo
    {
      Estado_Atual = 1;//Mantém amarelo (aplica histerese positiva)
    } 
    else //Caso 3: Zona de histerese sem estado anterior amarelo
    {
      Estado_Atual = 0;//Transição para vermelho (histerese negativa)
    }
  }
  else 
  {
    Estado_Atual = 0;//Estado atual da faixa Vermelho
  }
  
  //Atualiza os LEDs baseado na maquina de estado
  switch (Estado_Atual) 
  {
    case 2: //Verde
      digitalWrite(LED_VD, HIGH);
      digitalWrite(LED_AM, LOW);
      digitalWrite(LED_VM, LOW);
      break;
      
    case 1: //Amarelo
      digitalWrite(LED_VD, LOW);
      digitalWrite(LED_AM, HIGH);
      digitalWrite(LED_VM, LOW);
      break;
      
    case 0: //Vermelho
    default:
      digitalWrite(LED_VD, LOW);
      digitalWrite(LED_AM, LOW);
      digitalWrite(LED_VM, HIGH);
      break;
  }
  Estado_Anterior = Estado_Atual;
  
  //Aguarda 100 milissegundos antes de fazer nova leitura
  delay(100);
}
