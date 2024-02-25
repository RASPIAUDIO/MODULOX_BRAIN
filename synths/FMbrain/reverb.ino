





#define PRINTF(...) printf(__VA_ARGS__)


static float rev_time = 1.0f;
static float rev_level = 0.0f;



struct comb_s
{
    float *buf;
    int p;
    float g;
    int lim;
};

static struct comb_s cf0 =
{
    NULL,
    0,
    0.805f,
    (int)(rev_time * l_CB0),
};

static struct comb_s cf1 =
{
    NULL,
    0,
    0.827f,
    (int)(rev_time * l_CB1),
};

static struct comb_s cf2 =
{
    NULL,
    0,
    0.783f,
    (int)(rev_time * l_CB2),
};

static struct comb_s cf3 =
{
    NULL,
    0,
    0.764f,
    (int)(rev_time * l_CB3),
};

static inline void Do_Comb(struct comb_s *cf, const float *inSample, float *outSample, int buffLen)
{
    struct comb_s cf2;
    memcpy(&cf2, cf, sizeof(cf2));
    for (int n = 0; n < buffLen; n++)
    {
        const float readback = cf2.buf[cf2.p];
        const float newV = readback * cf2.g + inSample[n];
        cf2.buf[cf2.p] = newV;
        cf2.p++;
        if (cf2.p >= cf2.lim)
        {
            cf2.p = 0;
        }
        outSample[n] += readback;
    }
    memcpy(cf, &cf2, sizeof(cf2));
}

struct allpass_s
{
    float *buf;
    int p;
    float g;
    int lim;
};

static struct allpass_s ap0 =
{
    NULL,
    0,
    0.7f,
    (int)(rev_time * l_AP0)
};

static struct allpass_s ap1 =
{
    NULL,
    0,
    0.7f,
    (int)(rev_time * l_AP1)
};

static struct allpass_s ap2 =
{
    NULL,
    0,
    0.7f,
    (int)(rev_time * l_AP2)
};

static inline void Do_Allpass(struct allpass_s *ap, const float *inSample, float *outSample, int buffLen)
{
    struct allpass_s ap2;
    memcpy(&ap2, ap, sizeof(ap2));

    for (int n = 0; n < buffLen; n++)
    {
        float readback = ap2.buf[ap2.p];
        readback += (-ap2.g) * inSample[n];
        const float newV = readback * ap2.g + inSample[n];
        ap2.buf[ap2.p] = newV;
        ap2.p++;
        if (ap2.p >= ap2.lim)
        {
            ap2.p = 0;
        }
        outSample[n] = readback;
    }
    memcpy(ap, &ap2, sizeof(ap2));
}

void Reverb_Process(float *signal_l, int buffLen)
{
    Reverb_Process(signal_l, signal_l, buffLen);
}

void Reverb_Process(const float *signal_l, float *out, int buffLen)
{
    static float inSample[96];
    for (int n = 0; n < buffLen; n++)
    {
        /* create mono sample */
        inSample[n] = signal_l[n]; /* it may cause unwanted audible effects */
    }
    float newsample[buffLen];
    memset(newsample, 0, sizeof(newsample));
    Do_Comb(&cf0, inSample, newsample, buffLen);
    Do_Comb(&cf1, inSample, newsample, buffLen);
    Do_Comb(&cf2, inSample, newsample, buffLen);
    Do_Comb(&cf3, inSample, newsample, buffLen);
    for (int n = 0; n < buffLen; n++)
    {
        newsample[n] *= 0.25f;
    }

    Do_Allpass(&ap0, newsample, newsample, buffLen);
    Do_Allpass(&ap1, newsample, newsample, buffLen);
    Do_Allpass(&ap2, newsample, newsample, buffLen);

    /* apply reverb level */
    const float level = rev_level;
    for (int n = 0; n < buffLen; n++)
    {
        newsample[n] *= level;
        out[n] = signal_l[n] + newsample[n];
    }
}

static int CombInit(float *buffer, int i, struct comb_s *cf, int len)
{
    cf->buf = &buffer[i];
    cf->lim = (int)(rev_time * len);
    return len;
}

static int AllpassInit(float *buffer, int i, struct allpass_s *ap, int len)
{
    ap->buf = &buffer[i];
    ap->lim = (int)(rev_time * len);
    return len;
}

void Reverb_Setup(float *buffer)
{
    if (buffer == NULL)
    {
        PRINTF("No memory to initialize Reverb!\n");
        return;
    }
    else
    {
        memset(buffer, 0, sizeof(float) * REV_BUFF_SIZE);
    }
    int i = 0;

    i += CombInit(buffer, i, &cf0, l_CB0);
    i += CombInit(buffer, i, &cf1, l_CB1);
    i += CombInit(buffer, i, &cf2, l_CB2);
    i += CombInit(buffer, i, &cf3, l_CB3);

    i += AllpassInit(buffer, i, &ap0, l_AP0);
    i += AllpassInit(buffer, i, &ap1, l_AP1);
    i += AllpassInit(buffer, i, &ap2, l_AP2);

    PRINTF("rev: %d, %d\n", i, REV_BUFF_SIZE);
    if (i != REV_BUFF_SIZE)
    {
        PRINTF("Error during initialization of Reverb!\n");
    }
    else
    {
        PRINTF("Reverb is ready!\n");
    }
}

void Reverb_SetLevel(uint8_t not_used __attribute__((unused)), float value)
{
    rev_level = value;
    Status_ValueChangedFloat("Reverb", "Level", rev_level);
}

void Reverb_SetLevelInt(uint8_t not_used, uint8_t value)
{
    float val_f = value;
    val_f *= 1.0f / 127.0f;
    Reverb_SetLevel(not_used, val_f);
}
