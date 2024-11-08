#ifndef reverb_h
#define reverb_h

#define l_CB0 3460
#define l_CB1 2988
#define l_CB2 3882
#define l_CB3 4312
#define l_AP0 480
#define l_AP1 161
#define l_AP2 46

#define REV_BUFF_SIZE   (l_CB0 + l_CB1 + l_CB2 + l_CB3 + l_AP0 + l_AP1 + l_AP2)


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

static void Do_Comb(struct comb_s *cf, const float *inSample, float *outSample, int buffLen)
{
    for (int n = 0; n < buffLen; n++)
    {
        float readback = cf->buf[cf->p];
        float newV = readback * cf->g + inSample[n];
        cf->buf[cf->p] = newV;
        cf->p++;
        if (cf->p >= cf->lim)
        {
            cf->p = 0;
        }
        outSample[n] += readback;
    }
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

static void Do_Allpass(struct allpass_s *ap, float *inSample, int buffLen)
{
    for (int n = 0; n < buffLen; n++)
    {
        float readback = ap->buf[ap->p];
        readback += (-ap->g) * inSample[n];
        float newV = readback * ap->g + inSample[n];
        ap->buf[ap->p] = newV;
        ap->p++;
        if (ap->p >= ap->lim)
        {
            ap->p = 0;
        }
        inSample[n] = readback;
    }
}

void Reverb_Process(float *signal_l, int buffLen)
{
    float inSample[buffLen];
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

    Do_Allpass(&ap0, newsample, buffLen);
    Do_Allpass(&ap1, newsample, buffLen);
    Do_Allpass(&ap2, newsample, buffLen);

    /* apply reverb level */
    for (int n = 0; n < buffLen; n++)
    {
        newsample[n] *= rev_level;
        signal_l[n] += newsample[n];
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
        Serial.printf("No memory to initialize Reverb!\n");
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

#if 0
    cf0.buf = &buffer[i];
    cf0.lim = (int)(rev_time * l_CB0);
    i += l_CB0;

    cf1.buf = &buffer[i];
    cf1.lim = (int)(rev_time * l_CB1);
    i += l_CB1;

    cf2.buf = &buffer[i];
    cf2.lim = (int)(rev_time * l_CB2);
    i += l_CB2;

    cf3.buf = &buffer[i];
    cf3.lim = (int)(rev_time * l_CB3);
    i += l_CB3;

    ap0.buf = &buffer[i];
    ap0.lim = (int)(rev_time * l_AP0);
    i += l_AP0;

    ap1.buf = &buffer[i];
    ap1.lim = (int)(rev_time * l_AP1);
    i += l_AP1;

    ap2.buf = &buffer[i];
    ap2.lim = (int)(rev_time * l_AP2);
    i += l_AP2;
#endif
    Serial.printf("rev: %d, %d\n", i, REV_BUFF_SIZE);
    if (i != REV_BUFF_SIZE)
    {
        Serial.printf("Error during initialization of Reverb!\n");
    }
    else
    {
        Serial.printf("Reverb is ready!\n");
    }
}

void Reverb_SetLevel(uint8_t not_used __attribute__((unused)), float value)
{
    rev_level = value;
}

#endif