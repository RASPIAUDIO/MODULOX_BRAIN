bool i2s_write_sample_32ch2(uint32_t sample)
{
    static size_t bytes_written = 0;
    //Serial.println((sample>>16));
    i2s_write((i2s_port_t)i2s_num, (const char *)&sample, 4, &bytes_written, portMAX_DELAY);

    if (bytes_written > 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}


/*
 * i2s configuration
 */

i2s_config_t i2s_config =
{
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX ),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    //.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    //.intr_alloc_flags = 0,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 128,
    .use_apll = 0,
    .tx_desc_auto_clear=true,
    .fixed_mclk=-1
};

i2s_pin_config_t pins =
{
    .bck_io_num = 5,
    .ws_io_num =  16,
    .data_out_num = 17,
    .data_in_num = -1
};

void setup_i2s()
{  
  Serial.printf("1");
  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
  Serial.printf("2");
  i2s_set_pin(i2s_num, &pins);
  Serial.printf("3");
  i2s_set_sample_rates(i2s_num, SAMPLE_RATE);
  Serial.printf("4");
  i2s_start(i2s_num);
  Serial.printf("5");
  i2s_set_clk(i2s_num, SAMPLE_RATE, (i2s_bits_per_sample_t)16, (i2s_channel_t)2);
  Serial.printf("6");
  i2s_stop(i2s_num);
  delay(1000);
  i2s_start(i2s_num);
}
