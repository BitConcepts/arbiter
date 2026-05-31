# ZRM Include Library

Reusable ZRM fragments for common patterns. Import them into your models:

```yaml
imports:
  - zrm://sensor_health       # Sensor staleness and validity patterns
  - zrm://estop               # Emergency stop pattern
  - zrm://pid_gains            # Standard PID gain facts
  - zrm://network_common       # Network link quality facts
  - zrm://safety_common        # Common safety hazards and goals
  - zrm://access_control       # Auth, rate limiting, CORS facts
```

The `zprojc` compiler resolves `zrm://` imports from this directory,
merging facts, rules, and actions into the importing model.
