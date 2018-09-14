using System;
using UnityEngine;
using UnityEngine.UI;

namespace FPNG
{
    public class PlayerInfo : MonoBehaviour
    {
        private int m_MaxHealth;
        private int m_MaxStamina;
        private int m_Health;
        private float m_HealthF;
        private int m_Stamina;
        private float m_StaminaF;
        private int m_Experience;
        private int m_Level;

        private RectTransform m_HealthUI;
        private RectTransform m_StaminaUI;
        private RectTransform m_ExperienceUI;
        private Text m_LevelUI;
        private float m_HealthUIWidth;
        private float m_StaminaUIWidth;
        private float m_ExperienceUIWidth;

        private Action<float> OnHealthPercentageChange = null;
        private Action OnDeath = null;
        private Action OnLevelUp = null;

        public void SetOnHealthPercentageChange(Action<float> action)
        {
            OnHealthPercentageChange = action;
        }

        public void SetOnDeath(Action action)
        {
            OnDeath = action;
        }

        public void SetOnLevelUp(Action action)
        {
            OnLevelUp = action;
        }

        public void ResetAll()
        {
            SetMaxHealth(100);
            SetMaxStamina(100);
            SetHealth(m_MaxHealth);
            SetStamina(m_MaxStamina);
            SetExperience(0);
            SetLevel(1);
        }

        private void Update()
        {
            // hacky Update() instead of Start() because parent object is enabled before children
            if(m_HealthUI == null && GameObject.Find("PlayerInfoHealth") != null)
            {
                m_HealthUI = GameObject.Find("PlayerInfoHealth").GetComponent<RectTransform>();
                m_StaminaUI = GameObject.Find("PlayerInfoStamina").GetComponent<RectTransform>();
                m_ExperienceUI = GameObject.Find("PlayerInfoExperience").GetComponent<RectTransform>();
                m_LevelUI = GameObject.Find("PlayerInfoLevel").GetComponent<Text>();
                m_HealthUIWidth = GameObject.Find("PlayerInfoHealthBackground").GetComponent<RectTransform>().sizeDelta.x;
                m_StaminaUIWidth = GameObject.Find("PlayerInfoStaminaBackground").GetComponent<RectTransform>().sizeDelta.x;
                m_ExperienceUIWidth = GameObject.Find("PlayerInfoExperienceBackground").GetComponent<RectTransform>().sizeDelta.x;

                ResetAll();
            }
        }

        #region Public setters

        public void SetMaxHealth(int value)
        {
            value = Mathf.Max(0, value);
            m_MaxHealth = value;

            OnHealthPercentageChange(GetHealthPercentage());

            UpdateHealthUI();
        }

        public void SetMaxStamina(int value)
        {
            value = Mathf.Max(0, value);
            m_MaxStamina = value;

            UpdateStaminaUI();
        }

        private void SetHealth(int value, bool updateF)
        {
            value = Mathf.Clamp(value, 0, m_MaxHealth);
            int oldHealth = m_Health;
            m_Health = value;
            if(updateF)
                m_HealthF = m_Health;

            OnHealthPercentageChange(GetHealthPercentage());

            if (OnDeath != null && oldHealth > 0 && m_Health == 0)
                OnDeath();

            UpdateHealthUI();
        }

        public void SetHealth(int value)
        {
            SetHealth(value, true);
        }

        public void SetHealth(float value)
        {
            m_HealthF = value;
            SetHealth((int)m_HealthF, false);
        }

        private void SetStamina(int value, bool updateF)
        {
            value = Mathf.Clamp(value, 0, m_MaxHealth);
            m_Stamina = value;
            if (updateF)
                m_StaminaF = m_Stamina;

            UpdateStaminaUI();
        }

        public void SetStamina(int value)
        {
            SetStamina(value, true);
        }

        public void SetStamina(float value)
        {
            m_StaminaF = value;
            SetStamina((int)m_StaminaF, false);
        }

        public void SetExperience(int value)
        {
            value = Mathf.Max(0, value);
            m_Experience = value;

            while (m_Experience >= 100)
            {
                m_Experience -= 100;
                AddLevel(1); // call multiple times for correct callbacks with exp >= 200
            }

            UpdateExperienceUI();
        }

        public void SetLevel(int value)
        {
            value = Mathf.Max(1, value);
            int oldLevel = m_Level;
            m_Level = value;

            if (OnLevelUp != null && m_Level > oldLevel)
            {
                OnLevelUp();
                SetHealth(m_MaxHealth);
                SetStamina(m_MaxStamina);
            }

            UpdateLevelUI();
        }

        #endregion

        #region Private UI updaters

        private void UpdateHealthUI()
        {
            if (m_HealthUI == null)
                return;

            m_HealthUI.sizeDelta = CalcBarSize(m_MaxHealth, m_Health, m_HealthUIWidth, m_HealthUI.sizeDelta.y);
        }

        private void UpdateStaminaUI()
        {
            if (m_StaminaUI == null)
                return;

            m_StaminaUI.sizeDelta = CalcBarSize(m_MaxStamina, m_Stamina, m_StaminaUIWidth, m_StaminaUI.sizeDelta.y);
        }

        private void UpdateExperienceUI()
        {
            if (m_ExperienceUI == null)
                return;

            m_ExperienceUI.sizeDelta = CalcBarSize(100, m_Experience, m_ExperienceUIWidth, m_ExperienceUI.sizeDelta.y);
        }

        private Vector2 CalcBarSize(float max, float current, float width, float height)
        {
            return new Vector2(1 / max * current * width, height);
        }

        private void UpdateLevelUI()
        {
            if (m_LevelUI == null)
                return;

            m_LevelUI.text = m_Level.ToString();
        }

        #endregion

        #region Public adders

        public void AddMaxHealth(int value)
        {
            SetMaxHealth(m_MaxHealth + value);
        }

        public void AddMaxStamina(int value)
        {
            SetMaxStamina(m_MaxStamina + value);
        }

        public void AddHealth(int value)
        {
            SetHealth(m_Health + value);
        }

        public void AddHealth(float value)
        {
            SetHealth(m_HealthF + value);
        }

        public void AddStamina(int value)
        {
            SetStamina(m_Stamina + value);
        }

        public void AddStamina(float value)
        {
            SetStamina(m_StaminaF + value);
        }

        public void AddExperience(int value)
        {
            SetExperience(m_Experience + value);
        }

        public void AddLevel(int value)
        {
            SetLevel(m_Level + value);
        }

        #endregion

        #region Public getters

        public int GetMaxHealth()
        {
            return m_MaxHealth;
        }

        public int GetMaxStamina()
        {
            return m_MaxStamina;
        }

        public int GetHealth()
        {
            return m_Health;
        }

        public int GetStamina()
        {
            return m_Stamina;
        }

        public int GetExperience()
        {
            return m_Experience;
        }

        public int GetLevel()
        {
            return m_Level;
        }

        public float GetHealthPercentage()
        {
            return Mathf.Clamp(100f / m_MaxHealth * m_Health, 0f, 100f);
        }

        #endregion
    }
}