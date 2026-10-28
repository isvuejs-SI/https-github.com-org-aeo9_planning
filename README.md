# https-github.com-org-aeo9_planning

ROS 2 package: [drone_safety_ops](drone_safety_ops/README.md).

Simulation-only safety supervisor. PX4 on the Pixhawk owns the final failsafes. The companion computer publishes verdicts and sends hold, RTL, land, or no-arm requests.

```bash
make -C drone_safety_ops test
```

That test does not need ROS 2 or PX4. Legal and operating limits are in the package README. No real flights until every permission is in hand.
