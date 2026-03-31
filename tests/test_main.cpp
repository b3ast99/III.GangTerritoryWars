#include "TestFramework.h"

void RunIniConfigTests(Test::Runner& t);
void RunSidecarFormatTests(Test::Runner& t);
void RunWarKillTrackerTests(Test::Runner& t);
void RunTerritoryAabbTests(Test::Runner& t);
void RunSaveSlotParserTests(Test::Runner& t);
void RunWaveDeathRuleTests(Test::Runner& t);
void RunKillCreditRuleTests(Test::Runner& t);
void RunWaveConfigTests(Test::Runner& t);
void RunGangInfoTests(Test::Runner& t);
void RunNeutralRevertTests(Test::Runner& t);
void RunIslandRuleTests(Test::Runner& t);
void RunAffiliationRuleTests(Test::Runner& t);
void RunTerritoryStateRuleTests(Test::Runner& t);
void RunActTransitionRuleTests(Test::Runner& t);

int main() {
    Test::Runner t;

    RunIniConfigTests(t);
    RunSidecarFormatTests(t);
    RunWarKillTrackerTests(t);
    RunTerritoryAabbTests(t);
    RunSaveSlotParserTests(t);
    RunWaveDeathRuleTests(t);
    RunKillCreditRuleTests(t);
    RunWaveConfigTests(t);
    RunGangInfoTests(t);
    RunNeutralRevertTests(t);
    RunIslandRuleTests(t);
    RunAffiliationRuleTests(t);
    RunTerritoryStateRuleTests(t);
    RunActTransitionRuleTests(t);

    return t.report();
}
