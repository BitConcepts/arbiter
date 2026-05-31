# ARB Include Library

Reusable ARB fragments for common patterns. Import them into your models:

```yaml
imports:
  - ARB://sensor_health       # Sensor staleness and validity patterns
  - ARB://estop               # Emergency stop pattern
  - ARB://pid_gains            # Standard PID gain facts
  - ARB://network_common       # Network link quality facts
  - ARB://safety_common        # Common safety hazards and goals
  - ARB://access_control       # Auth, rate limiting, CORS facts
```

The `arbiterc` compiler resolves `ARB://` imports from this directory,
merging facts, rules, and actions into the importing model.
