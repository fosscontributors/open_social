@api @javascript
Feature: Add event to calendar
  Benefit: Ability to add event to calendar
  Role: As a Verified
  Goal/desire: I want to add event to my calendar

  Background:
    Given I enable the module "social_event_addtocal"

  Scenario: LU with "UTC" timezone can add event to "iCal" calendar
    Given add to calendar is enabled for "iCal"

    And users:
      | name               | mail             | status | timezone | roles    |
      | regular_user       | some@example.com | 1      | UTC      |verified |
    And events with non-anonymous author:
      | title               | body        | field_content_visibility | field_event_date    | field_event_date_end | langcode |
      | Walking in the park | lorem ipsum | public                   | 2100-01-01T10:00:00 | 2100-01-01T12:00:00  | en       |

    When I am logged in as "regular_user"
    And I am viewing the event "Walking in the park"
    And the file downloaded from "iCalendar" should contain individual lines:
      """
      BEGIN:VCALENDAR
      VERSION:2.0
      PRODID:Event
      METHOD:PUBLISH
      BEGIN:VEVENT
      SUMMARY:Walking in the park
      BEGIN:VTIMEZONE
      TZID:UTC
      BEGIN:STANDARD
      DTSTART:21000101T120000
      TZNAME:UTC
      TZOFFSETTO:+0000
      TZOFFSETFROM:+0000
      END:STANDARD
      END:VTIMEZONE
      DTSTART;TZID=UTC:21000101T100000
      DTEND;TZID=UTC:21000101T120000
      TRANSP:OPAQUE
      END:VEVENT
      END:VCALENDAR
      """

  Scenario: Anonymous enrolled to event can add event to "iCal" calendar
    Given add to calendar is enabled for "iCal"
    And I enable the module "social_event_an_enroll"

    And events with non-anonymous author:
      | title               | body        | field_content_visibility | field_event_date    | field_event_date_end | langcode | field_event_an_enroll |
      | Walking in the park | lorem ipsum | public                   | 2100-01-01T10:00:00 | 2100-01-01T12:00:00  | en       | 1                     |

    And I am viewing the event "Walking in the park"

    When I click "Enroll"
    And I wait for AJAX to finish
    And I wait for field: "Enroll as guest" of type: "link" to be rendered
    And I click "Enroll as guest"
    And I wait for AJAX to finish
    And I wait for field: "field_first_name" of type: "input" to be rendered
    And I fill in the following:
      | First name    | John         |
      | Last name     | Doe          |
      | Email address | john@doe.com |
    And I press "Enroll in event" in the "Modal"
    And I wait for AJAX to finish
    And the file downloaded from "iCalendar" should contain individual lines:
      """
      BEGIN:VCALENDAR
      VERSION:2.0
      PRODID:Event
      METHOD:PUBLISH
      BEGIN:VEVENT
      SUMMARY:Walking in the park
      BEGIN:VTIMEZONE
      TZID:UTC
      BEGIN:STANDARD
      DTSTART:21000101T120000
      TZNAME:UTC
      TZOFFSETTO:+0000
      TZOFFSETFROM:+0000
      END:STANDARD
      END:VTIMEZONE
      DTSTART;TZID=UTC:21000101T100000
      DTEND;TZID=UTC:21000101T120000
      TRANSP:OPAQUE
      END:VEVENT
      END:VCALENDAR
      """
