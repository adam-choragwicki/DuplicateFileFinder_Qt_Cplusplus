# Deployment Playbook

1. Confirm that the release candidate passed unit, integration, and migration tests.
2. Export the current production configuration and verify that the backup can be read.
3. Enable maintenance messaging five minutes before the database migration begins.
4. Apply schema changes, deploy the application services, and run the smoke-test checklist.
5. Compare error rates, response latency, and background-job depth with the pre-release baseline.
6. Remove maintenance messaging after the release lead and support lead approve the verification report.
7. Record the deployment identifier, completion time, and any follow-up actions in the release log.

If a blocking verification fails, stop the rollout and restore the previous application and configuration pair.
