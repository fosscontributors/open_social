@api
Feature: Edit my group as a group manager
  Benefit: Have control over adding members as Group Manager regardless of join method
  Role: As a GM
  Goal/desire: I want to change join method of my flexible group and still be able to add members

  Scenario: Successfully change join method of my group as a group manager and still be able to add members
    Given users:
      | name              | mail             | field_profile_organization | status | roles    |
      | Group Manager     | gm_1@example.com | GoalGorilla                | 1      | verified |

    And I am logged in as "Group Manager"
    And I am on "group/add/flexible_group"

    When I click radio button "Community" with the id "edit-field-flexible-group-visibility-community"
    And I fill in "Title" with "Test flexible group"
    And I show hidden inputs

    Then I click radio button "Open to join" with the id "edit-field-group-allowed-join-method-direct"
    And I fill in the "edit-field-group-description-0-value" WYSIWYG editor with "Description text"
    And I press "Save"
    And I should see "Test flexible group" in the "Main content"
    And I should see "1 member"
    And I click "Manage members"
    And I should see "Add members"

    # TB-4365 - As a Group Manager I want to change group join-method
    And I click "Edit group"
    And I wait for AJAX to finish
    And I click radio button "Invite only" with the id "edit-field-group-allowed-join-method-added"
    And I press "Save"
    And I click "Manage members"
    And I should see "Add members"
