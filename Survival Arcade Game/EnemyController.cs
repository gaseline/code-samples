using UnityEngine;

namespace FPNG
{
    [RequireComponent(typeof(AudioSource))]
    public abstract class EnemyController : Controller
    {
        // ...

        private void Start()
        {
            GlobalEnemyAIController.SetEnemyAI(this);
        }

        private void OnDestroy()
        {
            GlobalEnemyAIController.UnsetEnemyAI(this);
        }

        // ...

        private void RotateToMoveDir()
        {
            if (m_MoveDir.x == 0f && m_MoveDir.z == 0f)
                return;

            Vector3 moveDirFlat = m_MoveDir;
            moveDirFlat.y = 0f;
            moveDirFlat.Normalize();

            float originalXRotation = m_MeshTransform.eulerAngles.x;

            Quaternion rotation = Quaternion.LookRotation(moveDirFlat, m_MeshTransform.up);

            // remove the mesh x rotation temporarily
            m_MeshTransform.eulerAngles = new Vector3(
                0f,
                m_MeshTransform.eulerAngles.y,
                0f);

            // apply smooth rotation using slerp
            m_MeshTransform.localRotation = Quaternion.Slerp(m_MeshTransform.localRotation, rotation, Time.deltaTime * 5f);

            // place back x and original random y rotation
            m_MeshTransform.eulerAngles = new Vector3(
                originalXRotation,
                m_MeshTransform.eulerAngles.y,
                0f);
        }

        private void ProcessKnockback()
        {
            if (m_Knockback)
            {
                m_KnockbackSpeedPrev -= (m_KnockbackSpeedPrev / 5f) * (Time.fixedDeltaTime * 60);
                m_MoveDir.x = m_KnockbackDir.x;
                m_MoveDir.z = m_KnockbackDir.z;

                // move along surface normal
                RaycastHit hitInfo;
                float moveDirY = m_MoveDir.y;
                Vector3 colliderPosition = new Vector3(
                    transform.position.x + m_CharacterController.center.x * transform.lossyScale.x,
                    transform.position.y + m_CharacterController.center.y * transform.lossyScale.y,
                    transform.position.z + m_CharacterController.center.z * transform.lossyScale.z);
                float radius = m_CharacterController.radius * transform.lossyScale.x;

                if (Physics.SphereCast(colliderPosition, radius, m_KnockbackDir, out hitInfo,
                        radius, ~((1 << 2) | (1 << 9)), QueryTriggerInteraction.Ignore))
                    m_MoveDir = Vector3.ProjectOnPlane(m_MoveDir, hitInfo.normal).normalized;
                m_MoveDir.y = moveDirY;

                m_MoveDir.x = m_MoveDir.x * m_KnockbackSpeedPrev;
                m_MoveDir.z = m_MoveDir.z * m_KnockbackSpeedPrev;

                if (m_KnockbackSpeedPrev < 5f)
                    m_Knockback = false;
            }
        }

        private void OnDeath()
        {
            Destroy(gameObject);
            m_FirstPersonController.RegisterKill(m_EnemyInfo.GetMaxHealth() / 10);
        }

        public override void Damage(int points)
        {
            m_EnemyInfo.AddHealth(-points);
        }

        public override void Knockback(Vector3 direction)
        {
            m_MoveDir = new Vector3(0f, m_JumpSpeed, 0f);
            m_Jumping = true;
            m_Knockback = true;
            m_KnockbackDir = direction;
            m_KnockbackSpeedPrev = m_KnockbackSpeed;
        }

        protected abstract bool ProcessAttackBehavior();
    }
}