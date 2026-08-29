# Demo fixture

This AI-generated deterministic fixture is intended for README screenshots and manual demonstrations. Start the application
with `DevelopmentConfigurationHelper` set to `DemoTest`; it loads these three scan roots:

- `Creative Studio`
- `Product Team`
- `Company Archive`

The default file-content scan should find exactly 10 duplicate groups containing 33 files in total.

| Shared document | Files in group |
| --- | ---: |
| Brand guidelines | 3 |
| Atlas launch roadmap | 4 |
| Primary logo | 4 |
| Product catalog | 3 |
| Default application settings | 3 |
| Onboarding checklist | 3 |
| Borealis 1.4 release notes | 3 |
| Deployment playbook | 3 |
| Spring campaign announcement | 3 |
| Product-photo metadata | 4 |

Files in a group deliberately use different names and live in different parts of the hierarchy. The remaining
files have unique contents and exist to make the directory tree resemble a small working organization.
