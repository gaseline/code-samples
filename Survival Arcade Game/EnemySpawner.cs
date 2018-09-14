using UnityEngine;

namespace FPNG
{
    public class EnemySpawner : MonoBehaviour
    {
        [SerializeField] private int m_EnemyId;
        [SerializeField] private float m_SpawnTimeMin;
        [SerializeField] private float m_SpawnTimeMax;

        private float m_Timer;
        private float m_PausedTimerOffset;

        private GameObject m_FPSController;
        private bool m_FirstFrame;

        private bool IsActive()
        {
            return m_FPSController != null && m_FPSController.activeInHierarchy;
        }

        private void Start()
        {
            m_FirstFrame = true;

            NextSpawnTime();
        }

        private void Update()
        {
            if (m_FirstFrame)
            {
                m_FirstFrame = false;
                m_FPSController = GameObject.Find("CanvasManager").GetComponent<CanvasManager>().GetFPSControllerReference();
            }

            if (!IsActive())
                return;

            if(Time.time > m_Timer)
            {
                NextSpawnTime();
                ProcessSpawn();
            }
        }

        private void NextSpawnTime()
        {
            m_Timer = Time.time + Random.Range(m_SpawnTimeMin, m_SpawnTimeMax);
        }

        private void ProcessSpawn()
        {
            GameObject enemyInstance = (GameObject)Instantiate(Enemy.EnemyPrefabs[m_EnemyId].prefab);
            enemyInstance.transform.position = transform.position + new Vector3(Random.Range(0, 1), 0, Random.Range(0, 1));
        }

        public void PauseTimer()
        {
            m_PausedTimerOffset = m_Timer - Time.time;
        }

        public void ResumeTimer()
        {
            m_Timer = Time.time + m_PausedTimerOffset;
        }
    }
}