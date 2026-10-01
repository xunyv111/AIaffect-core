using UnityEngine;

namespace MyAiTown.AffectCore
{
    /// <summary>Small Unity JsonUtility boundary; game logic should call AffectRuntime directly.</summary>
    public sealed class AffectJsonAdapter
    {
        private readonly AffectRuntime runtime;

        public AffectJsonAdapter(AffectRuntime runtime)
        {
            this.runtime = runtime;
        }

        public ApplyResult TryApplyUpdateJson(string json)
        {
            if (string.IsNullOrWhiteSpace(json)) return new ApplyResult { ok = false, error = "json_empty" };
            try
            {
                var update = JsonUtility.FromJson<AffectUpdate>(json);
                return runtime.Apply(update);
            }
            catch
            {
                return new ApplyResult { ok = false, error = "json_invalid" };
            }
        }
    }
}
