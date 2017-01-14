using UnityEngine;
using UnityEngine.Audio;
using System.Collections;

public class EngineControl : MonoBehaviour
{
    public AudioMixer mixer;

	private float getruntime = 0f;
	private float finalENV = 0F;
	private float Vline = 0f;
	private float Vline2 = 0f;
	private float Vlineadd = 0f;
	private float mincount = 0f;
	private float maxcount = 0f;

    void Start()
    {
		mixer.GetFloat ("sendRunTime", out getruntime);
		Vline = 0f;
		Vlineadd = 1000 / (getruntime * 50);
    }

    void Update()
    {
		if (Input.GetMouseButton (0)) {
			Vline += Vlineadd;
			Vline2 = Vline * 2;
			mincount = Mathf.Min (1, Vline2);
			maxcount = Mathf.Max (1, Vline2);
			finalENV = (Mathf.Pow (1 - mincount, 6) + (maxcount - 1)) * (-1) + 1;

			mixer.SetFloat ("getENV", finalENV);
		}

		else {

			Vline = 0f;
			mixer.SetFloat ("getENV", 0f);
		}
    }
}
