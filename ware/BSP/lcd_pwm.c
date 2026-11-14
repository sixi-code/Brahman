//背光
#include "stm32f4xx.h"
#include "variables.h"
static uint16_t PWM_PSC = 60;     // 预分频值
static uint16_t PWM_ARR = 1400;   // 自动重装载值


// 屏幕背光PWM初始化
void LCD_TIM8_PWM_Init(void)
{

    //配置GPIOC的时钟
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOC, &GPIO_InitStruct);
    
    //配置TIM8的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM8);
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
    TIM_TimeBaseInitStruct.TIM_Period = PWM_ARR - 1;       // 自动重装载值
    TIM_TimeBaseInitStruct.TIM_Prescaler = PWM_PSC - 1;    // 预分频值
    TIM_TimeBaseInitStruct.TIM_ClockDivision = 0;          // 时钟分频
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseInitStruct);

    //配置TIM8的PWM模式
    TIM_OCInitTypeDef TIM_OCInitStruct;
    TIM_OCStructInit(&TIM_OCInitStruct);
    TIM_OCInitStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStruct.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OC1Init(TIM8, &TIM_OCInitStruct);//配置TIM8的通道1为PWM模式1，输出使能，极性低电平有效
    
    TIM_CtrlPWMOutputs(TIM8, ENABLE); //使能主输出
    
    TIM_Cmd(TIM8, ENABLE); //启动定时器

    g_pwm_inited = 1;//标志位，PWM初始化完成
}

// 关闭PWM，并配置引脚为推挽输出低电平
void LCD_PWM_DeInit(void)
{
    TIM_Cmd(TIM8, DISABLE);
    TIM_CtrlPWMOutputs(TIM8, DISABLE);
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_ResetBits(GPIOC, GPIO_Pin_6);//将引脚拉低

    g_pwm_inited = 0;//标志位，PWM未初始化
}

// 设置PWM占空比,设置背光亮度
void LCD_PWM_SetDutyCycle(uint16_t duty)
{
    /* CCR1 = 5*(duty+5), ARR = 1399。
     * duty > 275 时 CCR1 > ARR: PWM1 模式下比较值超出周期, 输出恒为有效电平,
     * 亮度直接阶跃到 100% 且不再随滑块变化(看起来像"调亮到头就没反应了")。
     * duty 上限钳到 275 即 5*(275+5)=1400 ≈ 满占空。 */
    if (duty > 275) duty = 275;
    TIM8->CCR1 = 5 * (duty + 5);
}

// 设置PWM频率
void LCD_PWM_SetFrequency(uint16_t frequency)
{
    /* frequency=0 会触发整数除零(Cortex-M4 上直接 UsageFault);
     * 大于 60000 时 (60000/frequency)-1 下溢成 0xFFFF, 实际频率掉到 1Hz 级,
     * 背光肉眼可见闪烁。 */
    if (frequency < 1 || frequency > 60000) return;
    TIM8->PSC = (60000 / frequency) - 1;
}